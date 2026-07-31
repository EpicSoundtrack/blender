/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "hydra/material.h"
#include "hydra/node_util.h"
#include "hydra/session.h"
#include "hydra/util.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/materialConnectionSchema.h>
#include <pxr/imaging/hd/materialNetworkSchema.h>
#include <pxr/imaging/hd/materialNodeParameterSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <array>
#include <climits>
#include <cmath>

HDCYCLES_NAMESPACE_OPEN_SCOPE

/* Normalize a material network node name to a full SdfPath. The schema may
 * provide either a full path or a bare identifier. */
static SdfPath MaterialNodeNameToSdfPath(const TfToken &nodeName)
{
  const std::string &s = nodeName.GetString();
  if (s.empty()) {
    return SdfPath::EmptyPath();
  }
  if (s[0] == '/' && SdfPath::IsValidPathString(s)) {
    return SdfPath(s);
  }
  return SdfPath::AbsoluteRootPath().AppendChild(nodeName);
}

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(CyclesMaterialTokens,
    (cycles)
    ((cyclesSurface, "cycles:surface"))
    ((cyclesDisplacement, "cycles:displacement"))
    ((cyclesVolume, "cycles:volume"))
    (UsdPreviewSurface)
    (UsdUVTexture)
    (UsdPrimvarReader_float)
    (UsdPrimvarReader_float2)
    (UsdPrimvarReader_float3)
    (UsdPrimvarReader_float4)
    (UsdPrimvarReader_int)
    (UsdTransform2d)
    (a)
    (rgb)
    (r)
    (g)
    (b)
    (result)
    (st)
    (wrapS)
    (wrapT)
    (periodic)
);
// clang-format on

/* Simple class to handle remapping of USDPreviewSurface nodes and parameters to Cycles
 * equivalents. */
class UsdToCyclesMapping {
 protected:
  using ParamMap = std::unordered_map<TfToken, ustring, TfToken::HashFunctor>;

 public:
  UsdToCyclesMapping(const char *nodeType, ParamMap paramMap)
      : _nodeType(nodeType), _paramMap(std::move(paramMap))
  {
  }

  ustring nodeType() const
  {
    return _nodeType;
  }

  virtual void initializeNode(ShaderNode * /*node*/) const {}

  virtual std::string parameterName(const TfToken &name,
                                    const ShaderInput *inputConnection,
                                    VtValue * /*value*/ = nullptr) const
  {
    /* UsdNode.name -> Node.input. These all follow a simple pattern that we can just
     * remap based on the name or 'Node.input' type. */
    if (inputConnection) {
      if (name == CyclesMaterialTokens->a) {
        return "alpha";
      }
      if (name == CyclesMaterialTokens->rgb) {
        return "color";
      }
      /* TODO: Is there a better mapping than 'color'? */
      if (name == CyclesMaterialTokens->r || name == CyclesMaterialTokens->g ||
          name == CyclesMaterialTokens->b)
      {
        return "color";
      }

      if (name == CyclesMaterialTokens->result) {
        switch (inputConnection->socket_type.type) {
          case SocketType::BOOLEAN:
          case SocketType::FLOAT:
          case SocketType::INT:
          case SocketType::UINT:
            return "alpha";
          case SocketType::COLOR:
          case SocketType::VECTOR:
          case SocketType::POINT:
          case SocketType::NORMAL:
          default:
            return "color";
        }
      }
    }

    /* Simple mapping case */
    const auto it = _paramMap.find(name);
    return it != _paramMap.end() ? it->second.string() : name.GetString();
  }

 private:
  const ustring _nodeType;
  ParamMap _paramMap;
};

class UsdToCyclesTexture : public UsdToCyclesMapping {
 public:
  using UsdToCyclesMapping::UsdToCyclesMapping;

  std::string parameterName(const TfToken &name,
                            const ShaderInput *inputConnection,
                            VtValue *value) const override
  {
    if (value) {
      /* Remap UsdUVTexture.wrapS and UsdUVTexture.wrapT to cycles_image_texture.extension. */
      if (name == CyclesMaterialTokens->wrapS || name == CyclesMaterialTokens->wrapT) {
        const std::string valueString = VtValue::Cast<std::string>(*value).Get<std::string>();

        /* A value of 'repeat' in USD is equivalent to 'periodic' in Cycles. */
        if (valueString == "repeat") {
          *value = VtValue(CyclesMaterialTokens->periodic);
        }

        return "extension";
      }
    }

    return UsdToCyclesMapping::parameterName(name, inputConnection, value);
  }
};

class UsdToCyclesMath : public UsdToCyclesMapping {
 public:
  UsdToCyclesMath(const NodeMathType math_type)
      : UsdToCyclesMapping("math",
                           {{TfToken("in"), ustring("value1")},
                            {TfToken("in1"), ustring("value1")},
                            {TfToken("in2"), ustring("value2")},
                            {TfToken("out"), ustring("Value")}}),
        _math_type(math_type)
  {
  }

  void initializeNode(ShaderNode *node) const override
  {
    static_cast<MathNode *>(node)->set_math_type(_math_type);
  }

 private:
  const NodeMathType _math_type;
};

class UsdToCyclesPowerMath : public UsdToCyclesMapping {
 public:
  UsdToCyclesPowerMath()
      : UsdToCyclesMapping("math",
                           {{TfToken("in"), ustring("value1")},
                            {TfToken("in1"), ustring("value1")},
                            {TfToken("in2"), ustring("value2")},
                            {TfToken("base"), ustring("value1")},
                            {TfToken("exponent"), ustring("value2")},
                            {TfToken("out"), ustring("Value")}})
  {
  }

  void initializeNode(ShaderNode *node) const override
  {
    static_cast<MathNode *>(node)->set_math_type(NODE_MATH_POWER);
  }
};

class UsdToCyclesVectorMath : public UsdToCyclesMapping {
 public:
  UsdToCyclesVectorMath(const NodeVectorMathType math_type, const bool scalar_output)
      : UsdToCyclesMapping("vector_math",
                           {{TfToken("in"), ustring("vector1")},
                            {TfToken("in1"), ustring("vector1")},
                            {TfToken("in2"), ustring("vector2")},
                            {TfToken("out"), scalar_output ? ustring("Value") : ustring("Vector")}}),
        _math_type(math_type)
  {
  }

  void initializeNode(ShaderNode *node) const override
  {
    static_cast<VectorMathNode *>(node)->set_math_type(_math_type);
  }

 private:
  const NodeVectorMathType _math_type;
};

class UsdToCyclesVectorGeometric : public UsdToCyclesMapping {
 public:
  UsdToCyclesVectorGeometric(const NodeVectorMathType math_type)
      : UsdToCyclesMapping("vector_math",
                           {{TfToken("in"), ustring("vector1")},
                            {TfToken("normal"), ustring("vector2")},
                            {TfToken("ior"), ustring("scale")},
                            {TfToken("out"), ustring("Vector")}}),
        _math_type(math_type)
  {
  }

  void initializeNode(ShaderNode *node) const override
  {
    static_cast<VectorMathNode *>(node)->set_math_type(_math_type);
  }

 private:
  const NodeVectorMathType _math_type;
};

namespace {

bool MaterialXIntegerLiteralParameter(HdMaterialNodeParameterContainerSchema params,
                                      const TfToken &name,
                                      int *value)
{
  const HdMaterialNodeParameterSchema param = params.Get(name);
  if (!param) {
    return false;
  }
  const auto data = param.GetValue();
  if (!data) {
    return false;
  }
  const VtValue vt_value = data->GetValue(0.0f);
  if (!vt_value.IsHolding<int>()) {
    return false;
  }
  *value = vt_value.UncheckedGet<int>();
  return true;
}

bool MaterialXFloatLiteralParameter(HdMaterialNodeParameterContainerSchema params,
                                    const TfToken &name,
                                    float *value)
{
  const HdMaterialNodeParameterSchema param = params.Get(name);
  if (!param) {
    return false;
  }
  const auto data = param.GetValue();
  if (!data) {
    return false;
  }
  const VtValue vt_value = data->GetValue(0.0f);
  if (!vt_value.IsHolding<float>()) {
    return false;
  }
  *value = vt_value.UncheckedGet<float>();
  return std::isfinite(*value);
}

bool MaterialXHasInputConnection(HdMaterialConnectionVectorContainerSchema connections,
                                 const TfToken &name)
{
  if (!connections) {
    return false;
  }
  const HdMaterialConnectionVectorSchema conn_vec = connections.Get(name);
  return conn_vec && conn_vec.GetNumElements() > 0;
}

bool MaterialXIntegerLiteralResult(const TfToken &node_type,
                                   HdMaterialNodeParameterContainerSchema params,
                                   HdMaterialConnectionVectorContainerSchema connections,
                                   int *result)
{
  const bool binary = node_type == TfToken("ND_add_integer") ||
                      node_type == TfToken("ND_subtract_integer");
  const bool unary = node_type == TfToken("ND_ceil_integer") ||
                     node_type == TfToken("ND_floor_integer") ||
                     node_type == TfToken("ND_round_integer");
  if (!binary && !unary) {
    return false;
  }

  if (binary) {
    if (MaterialXHasInputConnection(connections, TfToken("in1")) ||
        MaterialXHasInputConnection(connections, TfToken("in2")))
    {
      TF_RUNTIME_ERROR(
          "MaterialX integer node '%s' requires literal integer inputs; linked inputs are unsupported",
          node_type.GetText());
      return false;
    }
    int in1 = 0;
    int in2 = 0;
    if (!MaterialXIntegerLiteralParameter(params, TfToken("in1"), &in1) ||
        !MaterialXIntegerLiteralParameter(params, TfToken("in2"), &in2))
    {
      TF_RUNTIME_ERROR("MaterialX integer node '%s' requires literal integer in1 and in2 parameters",
                       node_type.GetText());
      return false;
    }
    if (node_type == TfToken("ND_add_integer")) {
      const long long sum = static_cast<long long>(in1) + static_cast<long long>(in2);
      if (sum < INT_MIN || sum > INT_MAX) {
        TF_RUNTIME_ERROR("MaterialX integer add result is outside supported 32-bit range");
        return false;
      }
      *result = int(sum);
      return true;
    }
    const long long difference = static_cast<long long>(in1) - static_cast<long long>(in2);
    if (difference < INT_MIN || difference > INT_MAX) {
      TF_RUNTIME_ERROR("MaterialX integer subtract result is outside supported 32-bit range");
      return false;
    }
    *result = int(difference);
    return true;
  }

  if (MaterialXHasInputConnection(connections, TfToken("in"))) {
    TF_RUNTIME_ERROR(
        "MaterialX integer node '%s' requires a literal float input; linked inputs are unsupported",
        node_type.GetText());
    return false;
  }
  float input = 0.0f;
  if (!MaterialXFloatLiteralParameter(params, TfToken("in"), &input)) {
    TF_RUNTIME_ERROR("MaterialX integer node '%s' requires a finite literal float in parameter",
                     node_type.GetText());
    return false;
  }
  const double rounded = node_type == TfToken("ND_floor_integer") ? std::floor(input) :
                         node_type == TfToken("ND_ceil_integer")  ? std::ceil(input) :
                                                                   std::round(input);
  if (rounded < INT_MIN || rounded > INT_MAX) {
    TF_RUNTIME_ERROR("MaterialX integer rounding result is outside supported 32-bit range");
    return false;
  }
  *result = int(rounded);
  return true;
}

bool MaterialXIntegerIsExactlyRepresentableAsFloat(const int value)
{
  return double(float(value)) == double(value);
}

class UsdToCycles {
  const UsdToCyclesMapping UsdPreviewSurface = {
      "principled_bsdf",
      {
          {TfToken("diffuseColor"), ustring("base_color")},
          {TfToken("emissiveColor"), ustring("emission")},
          {TfToken("specularColor"), ustring("specular")},
          {TfToken("clearcoatRoughness"), ustring("coat_roughness")},
          {TfToken("opacity"), ustring("alpha")},
          /* opacityThreshold */
          /* occlusion */
          /* displacement */
      }};
  const UsdToCyclesTexture UsdUVTexture = {
      "image_texture",
      {
          {CyclesMaterialTokens->st, ustring("vector")},
          {CyclesMaterialTokens->wrapS, ustring("extension")},
          {CyclesMaterialTokens->wrapT, ustring("extension")},
          {TfToken("file"), ustring("filename")},
          {TfToken("sourceColorSpace"), ustring("colorspace")},
      }};
  const UsdToCyclesMapping UsdPrimvarReader = {"attribute",
                                               {{TfToken("varname"), ustring("attribute")}}};
  const UsdToCyclesMapping MaterialXConstantColor3 = {
      "color", {{TfToken("value"), ustring("value")}, {TfToken("out"), ustring("Color")}}};
  const UsdToCyclesMapping MaterialXConstantFloat = {
      "value", {{TfToken("value"), ustring("value")}, {TfToken("out"), ustring("Value")}}};
  const UsdToCyclesMapping MaterialXMixColor3 = {
      "mix_color",
      {{TfToken("mix"), ustring("fac")},
       {TfToken("bg"), ustring("a")},
       {TfToken("fg"), ustring("b")},
       {TfToken("out"), ustring("result")}}};
  const UsdToCyclesMapping MaterialXImageColor3 = {
      "image_texture",
      {{TfToken("file"), ustring("filename")},
       {TfToken("texcoord"), ustring("vector")},
       {TfToken("out"), ustring("Color")}}};
  const UsdToCyclesMath MaterialXAbsvalFloat = {NODE_MATH_ABSOLUTE};
  const UsdToCyclesMath MaterialXAcosFloat = {NODE_MATH_ARCCOSINE};
  const UsdToCyclesMath MaterialXAddFloat = {NODE_MATH_ADD};
  const UsdToCyclesMath MaterialXAsinFloat = {NODE_MATH_ARCSINE};
  const UsdToCyclesMath MaterialXAtanFloat = {NODE_MATH_ARCTANGENT};
  const UsdToCyclesMath MaterialXCeilFloat = {NODE_MATH_CEIL};
  const UsdToCyclesMath MaterialXCosFloat = {NODE_MATH_COSINE};
  const UsdToCyclesMath MaterialXDivideFloat = {NODE_MATH_DIVIDE};
  const UsdToCyclesMath MaterialXExpFloat = {NODE_MATH_EXPONENT};
  const UsdToCyclesMath MaterialXFloorFloat = {NODE_MATH_FLOOR};
  const UsdToCyclesMath MaterialXFractFloat = {NODE_MATH_FRACTION};
  const UsdToCyclesMath MaterialXMaxFloat = {NODE_MATH_MAXIMUM};
  const UsdToCyclesMath MaterialXMinFloat = {NODE_MATH_MINIMUM};
  const UsdToCyclesMath MaterialXModuloFloat = {NODE_MATH_MODULO};
  const UsdToCyclesMath MaterialXMultiplyFloat = {NODE_MATH_MULTIPLY};
  const UsdToCyclesPowerMath MaterialXPowerFloat;
  const UsdToCyclesMath MaterialXRoundFloat = {NODE_MATH_ROUND};
  const UsdToCyclesMath MaterialXSignFloat = {NODE_MATH_SIGN};
  const UsdToCyclesMath MaterialXSinFloat = {NODE_MATH_SINE};
  const UsdToCyclesMath MaterialXSqrtFloat = {NODE_MATH_SQRT};
  const UsdToCyclesMath MaterialXSubtractFloat = {NODE_MATH_SUBTRACT};
  const UsdToCyclesMath MaterialXTanFloat = {NODE_MATH_TANGENT};
  const UsdToCyclesVectorMath MaterialXAddVector3 = {NODE_VECTOR_MATH_ADD, false};
  const UsdToCyclesVectorMath MaterialXSubtractVector3 = {NODE_VECTOR_MATH_SUBTRACT, false};
  const UsdToCyclesVectorMath MaterialXMultiplyVector3 = {NODE_VECTOR_MATH_MULTIPLY, false};
  const UsdToCyclesVectorMath MaterialXDivideVector3 = {NODE_VECTOR_MATH_DIVIDE, false};
  const UsdToCyclesVectorMath MaterialXCrossproductVector3 = {NODE_VECTOR_MATH_CROSS_PRODUCT, false};
  const UsdToCyclesVectorGeometric MaterialXReflectVector3 = {NODE_VECTOR_MATH_REFLECT};
  const UsdToCyclesVectorGeometric MaterialXRefractVector3 = {NODE_VECTOR_MATH_REFRACT};
  const UsdToCyclesVectorMath MaterialXDotproductVector3 = {NODE_VECTOR_MATH_DOT_PRODUCT, true};
  const UsdToCyclesVectorMath MaterialXDistanceVector3 = {NODE_VECTOR_MATH_DISTANCE, true};
  const UsdToCyclesVectorMath MaterialXMagnitudeVector3 = {NODE_VECTOR_MATH_LENGTH, true};
  const UsdToCyclesVectorMath MaterialXNormalizeVector3 = {NODE_VECTOR_MATH_NORMALIZE, false};
  const UsdToCyclesVectorMath MaterialXAbsvalVector3 = {NODE_VECTOR_MATH_ABSOLUTE, false};
  const UsdToCyclesVectorMath MaterialXFloorVector3 = {NODE_VECTOR_MATH_FLOOR, false};
  const UsdToCyclesVectorMath MaterialXCeilVector3 = {NODE_VECTOR_MATH_CEIL, false};
  const UsdToCyclesVectorMath MaterialXFractVector3 = {NODE_VECTOR_MATH_FRACTION, false};
  const UsdToCyclesVectorMath MaterialXSinVector3 = {NODE_VECTOR_MATH_SINE, false};
  const UsdToCyclesVectorMath MaterialXCosVector3 = {NODE_VECTOR_MATH_COSINE, false};
  const UsdToCyclesVectorMath MaterialXTanVector3 = {NODE_VECTOR_MATH_TANGENT, false};
  const UsdToCyclesVectorMath MaterialXSignVector3 = {NODE_VECTOR_MATH_SIGN, false};
  const UsdToCyclesVectorMath MaterialXMinVector3 = {NODE_VECTOR_MATH_MINIMUM, false};
  const UsdToCyclesVectorMath MaterialXMaxVector3 = {NODE_VECTOR_MATH_MAXIMUM, false};
  const std::array<std::pair<TfToken, const UsdToCyclesMapping *>, 22> MaterialXScalarMath = {{
      {TfToken("ND_absval_float"), &MaterialXAbsvalFloat},
      {TfToken("ND_acos_float"), &MaterialXAcosFloat},
      {TfToken("ND_add_float"), &MaterialXAddFloat},
      {TfToken("ND_asin_float"), &MaterialXAsinFloat},
      {TfToken("ND_atan_float"), &MaterialXAtanFloat},
      {TfToken("ND_ceil_float"), &MaterialXCeilFloat},
      {TfToken("ND_cos_float"), &MaterialXCosFloat},
      {TfToken("ND_divide_float"), &MaterialXDivideFloat},
      {TfToken("ND_exp_float"), &MaterialXExpFloat},
      {TfToken("ND_floor_float"), &MaterialXFloorFloat},
      {TfToken("ND_fract_float"), &MaterialXFractFloat},
      {TfToken("ND_max_float"), &MaterialXMaxFloat},
      {TfToken("ND_min_float"), &MaterialXMinFloat},
      {TfToken("ND_modulo_float"), &MaterialXModuloFloat},
      {TfToken("ND_multiply_float"), &MaterialXMultiplyFloat},
      {TfToken("ND_power_float"), &MaterialXPowerFloat},
      {TfToken("ND_round_float"), &MaterialXRoundFloat},
      {TfToken("ND_sign_float"), &MaterialXSignFloat},
      {TfToken("ND_sin_float"), &MaterialXSinFloat},
      {TfToken("ND_sqrt_float"), &MaterialXSqrtFloat},
      {TfToken("ND_subtract_float"), &MaterialXSubtractFloat},
      {TfToken("ND_tan_float"), &MaterialXTanFloat},
  }};
  const std::array<std::pair<TfToken, const UsdToCyclesMapping *>, 34> MaterialXVectorMath = {{
      {TfToken("ND_add_vector3"), &MaterialXAddVector3},
      {TfToken("ND_subtract_vector3"), &MaterialXSubtractVector3},
      {TfToken("ND_multiply_vector3"), &MaterialXMultiplyVector3},
      {TfToken("ND_divide_vector3"), &MaterialXDivideVector3},
      {TfToken("ND_crossproduct_vector3"), &MaterialXCrossproductVector3},
      {TfToken("ND_reflect_vector3"), &MaterialXReflectVector3},
      {TfToken("ND_refract_vector3"), &MaterialXRefractVector3},
      {TfToken("ND_dotproduct_vector3"), &MaterialXDotproductVector3},
      {TfToken("ND_distance_vector3"), &MaterialXDistanceVector3},
      {TfToken("ND_magnitude_vector3"), &MaterialXMagnitudeVector3},
      {TfToken("ND_normalize_vector3"), &MaterialXNormalizeVector3},
      {TfToken("ND_absval_vector3"), &MaterialXAbsvalVector3},
      {TfToken("ND_floor_vector3"), &MaterialXFloorVector3},
      {TfToken("ND_ceil_vector3"), &MaterialXCeilVector3},
      {TfToken("ND_fract_vector3"), &MaterialXFractVector3},
      {TfToken("ND_sin_vector3"), &MaterialXSinVector3},
      {TfToken("ND_cos_vector3"), &MaterialXCosVector3},
      {TfToken("ND_tan_vector3"), &MaterialXTanVector3},
      {TfToken("ND_sign_vector3"), &MaterialXSignVector3},
      {TfToken("ND_min_vector3"), &MaterialXMinVector3},
      {TfToken("ND_max_vector3"), &MaterialXMaxVector3},
      /* Cycles has no float2 math socket. The existing float2 adapter carries
       * MaterialX vector2 as (x, y, 0); all seven unary operations below
       * preserve zero, so this maps only its two defined components. */
      {TfToken("ND_floor_vector2"), &MaterialXFloorVector3},
      {TfToken("ND_ceil_vector2"), &MaterialXCeilVector3},
      {TfToken("ND_fract_vector2"), &MaterialXFractVector3},
      {TfToken("ND_sin_vector2"), &MaterialXSinVector3},
      {TfToken("ND_cos_vector2"), &MaterialXCosVector3},
      {TfToken("ND_tan_vector2"), &MaterialXTanVector3},
      {TfToken("ND_sign_vector2"), &MaterialXSignVector3},
      /* These component-wise Vector Math operations also preserve the exact
       * (x, y, 0) Vector2 adapter: zero remains zero in the adapter channel. */
      {TfToken("ND_absval_vector2"), &MaterialXAbsvalVector3},
      {TfToken("ND_add_vector2"), &MaterialXAddVector3},
      {TfToken("ND_subtract_vector2"), &MaterialXSubtractVector3},
      {TfToken("ND_multiply_vector2"), &MaterialXMultiplyVector3},
      {TfToken("ND_min_vector2"), &MaterialXMinVector3},
      {TfToken("ND_max_vector2"), &MaterialXMaxVector3},
  }};

 public:
  const UsdToCyclesMapping *findUsd(const TfToken &usdNodeType)
  {
    if (usdNodeType == CyclesMaterialTokens->UsdPreviewSurface) {
      return &UsdPreviewSurface;
    }
    if (usdNodeType == CyclesMaterialTokens->UsdUVTexture) {
      return &UsdUVTexture;
    }
    if (usdNodeType == TfToken("ND_constant_color3")) {
      return &MaterialXConstantColor3;
    }
    if (usdNodeType == TfToken("ND_constant_float")) {
      return &MaterialXConstantFloat;
    }
    if (usdNodeType == TfToken("ND_mix_color3")) {
      return &MaterialXMixColor3;
    }
    if (usdNodeType == TfToken("ND_image_color3")) {
      return &MaterialXImageColor3;
    }
    for (const auto &[materialx_type, mapping] : MaterialXScalarMath) {
      if (usdNodeType == materialx_type) {
        return mapping;
      }
    }
    for (const auto &[materialx_type, mapping] : MaterialXVectorMath) {
      if (usdNodeType == materialx_type) {
        return mapping;
      }
    }
    if (usdNodeType == CyclesMaterialTokens->UsdPrimvarReader_float ||
        usdNodeType == CyclesMaterialTokens->UsdPrimvarReader_float2 ||
        usdNodeType == CyclesMaterialTokens->UsdPrimvarReader_float3 ||
        usdNodeType == CyclesMaterialTokens->UsdPrimvarReader_float4 ||
        usdNodeType == CyclesMaterialTokens->UsdPrimvarReader_int)
    {
      return &UsdPrimvarReader;
    }

    return nullptr;
  }
  const UsdToCyclesMapping *findCycles(const ustring & /*cyclesNodeType*/)
  {
    return nullptr;
  }
};
TfStaticData<UsdToCycles> sUsdToCyles;

}  // namespace

HdCyclesMaterial::HdCyclesMaterial(const SdfPath &sprimId) : HdMaterial(sprimId) {}

HdCyclesMaterial::~HdCyclesMaterial() = default;

HdDirtyBits HdCyclesMaterial::GetInitialDirtyBitsMask() const
{
  return DirtyBits::DirtyResource | DirtyBits::DirtyParams;
}

void HdCyclesMaterial::Sync(HdSceneDelegate *sceneDelegate,
                            HdRenderParam *renderParam,
                            HdDirtyBits *dirtyBits)
{
  if (*dirtyBits == DirtyBits::Clean) {
    return;
  }

  Initialize(renderParam);

  const SceneLock lock(renderParam);

  const bool dirtyParams = (*dirtyBits & DirtyBits::DirtyParams);
  const bool dirtyResource = (*dirtyBits & DirtyBits::DirtyResource);

  const SdfPath &id = GetId();

  if (dirtyResource || dirtyParams) {
    const HdSceneIndexPrim prim = GetPrim(sceneDelegate, id);
    const HdContainerDataSourceHandle &primDs = prim.dataSource;

    HdMaterialSchema matSchema = HdMaterialSchema::GetFromParent(primDs);
    /* Prefer cycles network if it exists, otherwise use universal network. */
    HdMaterialNetworkSchema network = matSchema.GetMaterialNetwork(CyclesMaterialTokens->cycles);
    if (!network) {
      network = matSchema.GetMaterialNetwork();
    }

    if (network) {
      if (!_nodes.empty() && !dirtyResource) {
        UpdateParameters(network);
        _shader->tag_modified();
      }
      else {
        PopulateShaderGraph(network);
      }
    }
    else {
      TF_RUNTIME_ERROR("Could not get a material network for %s.", id.GetText());
    }
  }

  if (_shader->is_modified()) {
    _shader->tag_update(lock.scene);
  }

  *dirtyBits = DirtyBits::Clean;
}

void HdCyclesMaterial::UpdateParameters(NodeDesc &nodeDesc,
                                        HdMaterialNodeParameterContainerSchema params,
                                        const SdfPath &nodePath)
{
  for (const TfToken &paramName : params.GetNames()) {
    if (nodeDesc.consumed_parameters.contains(paramName)) {
      continue;
    }
    auto valueDs = params.Get(paramName).GetValue();
    if (!valueDs) {
      continue;
    }
    VtValue value = valueDs->GetValue(0.0f);

    /* See if the parameter name is in USDPreviewSurface terms, and needs to be converted .*/
    const UsdToCyclesMapping *inputMapping = nodeDesc.mapping;
    const std::string inputName = inputMapping ?
                                      inputMapping->parameterName(paramName, nullptr, &value) :
                                      paramName.GetString();

    const auto endpointIt = nodeDesc.input_endpoints.find(paramName);
    if (endpointIt != nodeDesc.input_endpoints.end()) {
      for (ShaderInput *input : endpointIt->second) {
        SetNodeValue(input->parent, input->socket_type, value);
      }
      continue;
    }

    /* Find the input to write the parameter value to. */
    const SocketType *input = nullptr;
    for (const SocketType &socket : nodeDesc.node->type->inputs) {
      if (string_iequals(socket.name.string(), inputName)) {
        input = &socket;
        break;
      }
    }

    if (!input) {
      TF_WARN("Could not find parameter '%s' on node '%s' ('%s')",
              paramName.GetText(),
              nodePath.GetText(),
              nodeDesc.node->name.c_str());
      continue;
    }

    SetNodeValue(nodeDesc.node, *input, value);
  }
}

void HdCyclesMaterial::UpdateParameters(HdMaterialNetworkSchema network)
{
  HdMaterialNodeContainerSchema nodes = network.GetNodes();
  for (const TfToken &nodeName : nodes.GetNames()) {
    const SdfPath nodePath = MaterialNodeNameToSdfPath(nodeName);

    const auto nodeIt = _nodes.find(nodePath);
    if (nodeIt == _nodes.end()) {
      TF_RUNTIME_ERROR("Could not update parameters on missing node '%s'", nodePath.GetText());
      continue;
    }

    UpdateParameters(nodeIt->second, nodes.Get(nodeName).GetParameters(), nodePath);
  }
}

void HdCyclesMaterial::UpdateConnections(NodeDesc &nodeDesc,
                                         HdMaterialNodeSchema nodeSchema,
                                         const SdfPath &nodePath,
                                         ShaderGraph *shaderGraph)
{
  HdMaterialConnectionVectorContainerSchema conns = nodeSchema.GetInputConnections();
  for (const TfToken &dstSocketName : conns.GetNames()) {
    HdMaterialConnectionVectorSchema connVec = conns.Get(dstSocketName);
    const size_t count = connVec.GetNumElements();
    if (count == 0) {
      continue;
    }

    const UsdToCyclesMapping *inputMapping = nodeDesc.mapping;
    const std::string inputName = inputMapping ?
                                      inputMapping->parameterName(dstSocketName, nullptr) :
                                      dstSocketName.GetString();

    std::vector<ShaderInput *> inputs;
    const auto endpointIt = nodeDesc.input_endpoints.find(dstSocketName);
    if (endpointIt != nodeDesc.input_endpoints.end()) {
      inputs = endpointIt->second;
    }
    else {
      for (ShaderInput *in : nodeDesc.node->inputs) {
        if (string_iequals(in->socket_type.name.string(), inputName)) {
          inputs.push_back(in);
          break;
        }
      }
    }

    if (inputs.empty()) {
      TF_WARN("Ignoring connection on '%s.%s', input '%s' was not found",
              nodePath.GetText(),
              dstSocketName.GetText(),
              dstSocketName.GetText());
      continue;
    }

    /* USD allows N connections per input (MaterialX <switch>, <combine>, struct
     * inputs etc). Cycles inputs are single-connection, and the right lowering
     * depends on the node type, so just take the first and warn. */
    if (count > 1) {
      TF_WARN(
          "Ignoring multiple connections to '%s.%s'", nodePath.GetText(), dstSocketName.GetText());
    }

    HdMaterialConnectionSchema connSchema = connVec.GetElement(0);
    const SdfPath upstreamNodePath =
        connSchema.GetUpstreamNodePath() ?
            MaterialNodeNameToSdfPath(connSchema.GetUpstreamNodePath()->GetTypedValue(0.0f)) :
            SdfPath();
    const TfToken upstreamOutputName = connSchema.GetUpstreamNodeOutputName() ?
                                           connSchema.GetUpstreamNodeOutputName()->GetTypedValue(
                                               0.0f) :
                                           TfToken();

    const auto srcNodeIt = _nodes.find(upstreamNodePath);
    if (srcNodeIt == _nodes.end()) {
      TF_WARN("Ignoring connection from '%s.%s' to '%s.%s', node '%s' was not found",
              upstreamNodePath.GetText(),
              upstreamOutputName.GetText(),
              nodePath.GetText(),
              dstSocketName.GetText(),
              upstreamNodePath.GetText());
      continue;
    }

    ShaderOutput *output = nullptr;
    const auto outputEndpointIt = srcNodeIt->second.output_endpoints.find(upstreamOutputName);
    if (outputEndpointIt != srcNodeIt->second.output_endpoints.end()) {
      output = outputEndpointIt->second;
    }
    else {
      const UsdToCyclesMapping *outputMapping = srcNodeIt->second.mapping;
      const std::string outputName = outputMapping ?
                                         outputMapping->parameterName(upstreamOutputName, inputs.front()) :
                                         upstreamOutputName.GetString();
      for (ShaderOutput *out : srcNodeIt->second.node->outputs) {
        if (string_iequals(out->socket_type.name.string(), outputName)) {
          output = out;
          break;
        }
      }
    }

    if (!output) {
      TF_WARN("Ignoring connection from '%s.%s' to '%s.%s', output '%s' was not found",
              upstreamNodePath.GetText(),
              upstreamOutputName.GetText(),
              nodePath.GetText(),
              dstSocketName.GetText(),
              upstreamOutputName.GetText());
      continue;
    }

    for (ShaderInput *input : inputs) {
      shaderGraph->connect(output, input);
    }
  }
}

void HdCyclesMaterial::PopulateShaderGraph(HdMaterialNetworkSchema network)
{
  _nodes.clear();

  unique_ptr<ShaderGraph> graph = make_unique<ShaderGraph>();

  HdMaterialNodeContainerSchema nodes = network.GetNodes();
  std::unordered_set<SdfPath, SdfPath::Hash> rejected_nodes;

  /* Iterate all the nodes first and build a complete but unconnected graph with parameters set. */
  for (const TfToken &nodeName : nodes.GetNames()) {
    HdMaterialNodeSchema nodeSchema = nodes.Get(nodeName);
    const SdfPath nodePath = MaterialNodeNameToSdfPath(nodeName);

    NodeDesc nodeDesc = {};

    const auto nodeIt = _nodes.find(nodePath);
    /* Create new node only if it does not exist yet. */
    if (nodeIt != _nodes.end()) {
      nodeDesc = nodeIt->second;
    }
    else {
      /* E.g. cycles_principled_bsdf or UsdPreviewSurface. */
      const TfToken nodeTypeIdToken = nodeSchema.GetNodeIdentifier() ?
                                          nodeSchema.GetNodeIdentifier()->GetTypedValue(0.0f) :
                                          TfToken();
      const std::string &nodeTypeId = nodeTypeIdToken.GetString();

      if (nodeTypeIdToken == TfToken("ND_add_integer") ||
          nodeTypeIdToken == TfToken("ND_subtract_integer") ||
          nodeTypeIdToken == TfToken("ND_ceil_integer") ||
          nodeTypeIdToken == TfToken("ND_floor_integer") ||
          nodeTypeIdToken == TfToken("ND_round_integer"))
      {
        int result = 0;
        if (!MaterialXIntegerLiteralResult(
                nodeTypeIdToken, nodeSchema.GetParameters(), nodeSchema.GetInputConnections(), &result))
        {
          rejected_nodes.insert(nodePath);
          continue;
        }
        if (!MaterialXIntegerIsExactlyRepresentableAsFloat(result)) {
          TF_RUNTIME_ERROR(
              "MaterialX integer node '%s' result cannot be represented exactly by Cycles Value",
              nodeTypeIdToken.GetText());
          rejected_nodes.insert(nodePath);
          continue;
        }
        ValueNode *value = graph->create_node<ValueNode>();
        value->set_value(float(result));
        nodeDesc.node = value;
        nodeDesc.output_endpoints[TfToken("out")] = value->output("Value");
        nodeDesc.consumed_parameters.insert(TfToken("in"));
        nodeDesc.consumed_parameters.insert(TfToken("in1"));
        nodeDesc.consumed_parameters.insert(TfToken("in2"));
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_noise3d_float")) {
        NoiseTextureNode *noise = graph->create_node<NoiseTextureNode>();
        noise->set_dimensions(3);
        MathNode *amplitude = graph->create_node<MathNode>();
        amplitude->set_math_type(NODE_MATH_MULTIPLY);
        MathNode *pivot = graph->create_node<MathNode>();
        pivot->set_math_type(NODE_MATH_ADD);
        graph->connect(noise->output("Fac"), amplitude->input("Value1"));
        graph->connect(amplitude->output("Value"), pivot->input("Value1"));
        nodeDesc.node = pivot;
        nodeDesc.input_endpoints[TfToken("position")] = {noise->input("Vector")};
        nodeDesc.input_endpoints[TfToken("amplitude")] = {amplitude->input("Value2")};
        nodeDesc.input_endpoints[TfToken("pivot")] = {pivot->input("Value2")};
        nodeDesc.output_endpoints[TfToken("out")] = pivot->output("Value");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_noise3d_color3") ||
          nodeTypeIdToken == TfToken("ND_noise3d_color3FA"))
      {
        NoiseTextureNode *noise = graph->create_node<NoiseTextureNode>();
        noise->set_dimensions(3);
        VectorMathNode *scale = graph->create_node<VectorMathNode>();
        scale->set_math_type(NODE_VECTOR_MATH_MULTIPLY);
        CombineXYZNode *pivot = graph->create_node<CombineXYZNode>();
        VectorMathNode *add = graph->create_node<VectorMathNode>();
        add->set_math_type(NODE_VECTOR_MATH_ADD);
        graph->connect(noise->output("Color"), scale->input("Vector1"));
        graph->connect(scale->output("Vector"), add->input("Vector1"));
        graph->connect(pivot->output("Vector"), add->input("Vector2"));
        nodeDesc.node = add;
        nodeDesc.input_endpoints[TfToken("position")] = {noise->input("Vector")};
        nodeDesc.input_endpoints[TfToken("amplitude")] =
            nodeTypeIdToken == TfToken("ND_noise3d_color3") ?
                std::vector<ShaderInput *>{scale->input("Vector2")} :
                std::vector<ShaderInput *>{scale->input("Vector2")};
        nodeDesc.input_endpoints[TfToken("pivot")] = {
            pivot->input("X"), pivot->input("Y"), pivot->input("Z")};
        nodeDesc.output_endpoints[TfToken("out")] = add->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_rgbtohsv_color3") ||
          nodeTypeIdToken == TfToken("ND_hsvtorgb_color3"))
      {
        const bool rgb_to_hsv = nodeTypeIdToken == TfToken("ND_rgbtohsv_color3");
        SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
        separate->set_color_type(rgb_to_hsv ? NODE_COMBSEP_COLOR_HSV : NODE_COMBSEP_COLOR_RGB);
        CombineColorNode *combine = graph->create_node<CombineColorNode>();
        combine->set_color_type(rgb_to_hsv ? NODE_COMBSEP_COLOR_RGB : NODE_COMBSEP_COLOR_HSV);
        for (const char *channel : {"Red", "Green", "Blue"}) {
          graph->connect(separate->output(channel), combine->input(channel));
        }

        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Color")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Color");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_constant_vector3")) {
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        for (const char *channel : {"X", "Y", "Z"}) {
          graph->connect(separate->output(channel), combine->input(channel));
        }

        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("value")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_constant_vector2")) {
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        combine->set_z(0.0f);
        graph->connect(separate->output("X"), combine->input("X"));
        graph->connect(separate->output("Y"), combine->input("Y"));
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("value")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_combine2_vector2")) {
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        combine->set_z(0.0f);
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in1")] = {combine->input("X")};
        nodeDesc.input_endpoints[TfToken("in2")] = {combine->input("Y")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_float_vector2")) {
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        combine->set_z(0.0f);
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {combine->input("X"), combine->input("Y")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_integer_vector2")) {
        if (nodeSchema.GetInputConnections().Get(TfToken("in")).GetNumElements() != 0) {
          TF_RUNTIME_ERROR("MaterialX integer conversion does not support linked input");
          continue;
        }
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        combine->set_z(0.0f);
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {combine->input("X"), combine->input("Y")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_separate2_vector2")) {
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        nodeDesc.node = separate;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("outx")] = separate->output("X");
        nodeDesc.output_endpoints[TfToken("outy")] = separate->output("Y");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_extract_vector2")) {
        int index = 0;
        if (const auto data = nodeSchema.GetParameters().Get(TfToken("index")).GetValue()) {
          const VtValue value = data->GetValue(0.0f);
          if (value.IsHolding<int>()) index = value.UncheckedGet<int>();
        }
        if (index < 0 || index > 1) {
          TF_RUNTIME_ERROR("MaterialX extract vector index must be 0 or 1");
          continue;
        }
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        nodeDesc.node = separate;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = separate->output(index == 0 ? "X" : "Y");
        nodeDesc.consumed_parameters.insert(TfToken("index"));
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_combine3_vector3")) {
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in1")] = {combine->input("X")};
        nodeDesc.input_endpoints[TfToken("in2")] = {combine->input("Y")};
        nodeDesc.input_endpoints[TfToken("in3")] = {combine->input("Z")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_float_color3")) {
        CombineColorNode *combine = graph->create_node<CombineColorNode>();
        combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {
            combine->input("Red"), combine->input("Green"), combine->input("Blue")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Color");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_integer_color3")) {
        if (nodeSchema.GetInputConnections().Get(TfToken("in")).GetNumElements() != 0) {
          TF_RUNTIME_ERROR("MaterialX integer conversion does not support linked input");
          continue;
        }
        CombineColorNode *combine = graph->create_node<CombineColorNode>();
        combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {
            combine->input("Red"), combine->input("Green"), combine->input("Blue")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Color");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_luminance_color3")) {
        SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
        separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        graph->connect(separate->output("Red"), combine->input("X"));
        graph->connect(separate->output("Green"), combine->input("Y"));
        graph->connect(separate->output("Blue"), combine->input("Z"));
        VectorMathNode *dot = graph->create_node<VectorMathNode>();
        dot->set_math_type(NODE_VECTOR_MATH_DOT_PRODUCT);
        graph->connect(combine->output("Vector"), dot->input("Vector1"));
        nodeDesc.node = dot;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Color")};
        nodeDesc.input_endpoints[TfToken("lumacoeffs")] = {dot->input("Vector2")};
        nodeDesc.output_endpoints[TfToken("out")] = dot->output("Value");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_extract_vector3")) {
        int index = 0;
        if (const auto data = nodeSchema.GetParameters().Get(TfToken("index")).GetValue()) {
          const VtValue value = data->GetValue(0.0f);
          if (value.IsHolding<int>()) index = value.UncheckedGet<int>();
        }
        if (index < 0 || index > 2) {
          TF_RUNTIME_ERROR("MaterialX extract vector3 index must be 0, 1, or 2");
          continue;
        }
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        nodeDesc.node = separate;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = separate->output(
            index == 0 ? "X" : index == 1 ? "Y" : "Z");
        nodeDesc.consumed_parameters.insert(TfToken("index"));
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_separate3_vector3")) {
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        nodeDesc.node = separate;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("outx")] = separate->output("X");
        nodeDesc.output_endpoints[TfToken("outy")] = separate->output("Y");
        nodeDesc.output_endpoints[TfToken("outz")] = separate->output("Z");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_extract_color3")) {
        int index = 0;
        if (const auto index_data = nodeSchema.GetParameters().Get(TfToken("index")).GetValue()) {
          const VtValue index_value = index_data->GetValue(0.0f);
          if (index_value.IsHolding<int>()) {
            index = index_value.UncheckedGet<int>();
          }
        }
        if (index < 0 || index > 2) {
          TF_RUNTIME_ERROR("MaterialX extract color index must be 0, 1, or 2");
          continue;
        }
        SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
        separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
        const char *channel = index == 0 ? "Red" : index == 1 ? "Green" : "Blue";
        nodeDesc.node = separate;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Color")};
        nodeDesc.output_endpoints[TfToken("out")] = separate->output(channel);
        nodeDesc.consumed_parameters.insert(TfToken("index"));
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_color3_vector3")) {
        SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
        separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        for (const auto &[color, vector] : {std::pair{"Red", "X"},
                                            std::pair{"Green", "Y"},
                                            std::pair{"Blue", "Z"}}) {
          graph->connect(separate->output(color), combine->input(vector));
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Color")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_color3_vector2")) {
        SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
        separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        combine->set_z(0.0f);
        graph->connect(separate->output("Red"), combine->input("X"));
        graph->connect(separate->output("Green"), combine->input("Y"));
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Color")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_vector3_color3")) {
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        CombineColorNode *combine = graph->create_node<CombineColorNode>();
        combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
        for (const auto &[vector, color] : {std::pair{"X", "Red"},
                                            std::pair{"Y", "Green"},
                                            std::pair{"Z", "Blue"}}) {
          graph->connect(separate->output(vector), combine->input(color));
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Color");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_vector2_color3")) {
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        CombineColorNode *combine = graph->create_node<CombineColorNode>();
        combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
        combine->set_b(0.0f);
        graph->connect(separate->output("X"), combine->input("Red"));
        graph->connect(separate->output("Y"), combine->input("Green"));
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Color");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_atan2_float")) {
        /* MaterialX orders atan2 arguments as atan2(iny, inx). */
        MathNode *math = graph->create_node<MathNode>();
        math->set_math_type(NODE_MATH_ARCTAN2);
        nodeDesc.node = math;
        nodeDesc.input_endpoints[TfToken("iny")] = {math->input("Value1")};
        nodeDesc.input_endpoints[TfToken("inx")] = {math->input("Value2")};
        nodeDesc.output_endpoints[TfToken("out")] = math->output("Value");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_invert_float")) {
        /* MaterialX invert is amount - in, not a blend operation. */
        MathNode *subtract = graph->create_node<MathNode>();
        subtract->set_math_type(NODE_MATH_SUBTRACT);
        nodeDesc.node = subtract;
        nodeDesc.input_endpoints[TfToken("amount")] = {subtract->input("Value1")};
        nodeDesc.input_endpoints[TfToken("in")] = {subtract->input("Value2")};
        nodeDesc.output_endpoints[TfToken("out")] = subtract->output("Value");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_ln_float")) {
        /* Cycles logarithm is log(Value1, Value2); MaterialX ln is base e. */
        MathNode *math = graph->create_node<MathNode>();
        math->set_math_type(NODE_MATH_LOGARITHM);
        math->set_value2(2.71828182845904523536f);
        nodeDesc.node = math;
        nodeDesc.input_endpoints[TfToken("in")] = {math->input("Value1")};
        nodeDesc.output_endpoints[TfToken("out")] = math->output("Value");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_clamp_float")) {
        /* MaterialX clamp is min(max(in, low), high). */
        MathNode *maximum = graph->create_node<MathNode>();
        maximum->set_math_type(NODE_MATH_MAXIMUM);
        MathNode *minimum = graph->create_node<MathNode>();
        minimum->set_math_type(NODE_MATH_MINIMUM);
        graph->connect(maximum->output("Value"), minimum->input("Value1"));
        nodeDesc.node = minimum;
        nodeDesc.input_endpoints[TfToken("in")] = {maximum->input("Value1")};
        nodeDesc.input_endpoints[TfToken("low")] = {maximum->input("Value2")};
        nodeDesc.input_endpoints[TfToken("high")] = {minimum->input("Value2")};
        nodeDesc.output_endpoints[TfToken("out")] = minimum->output("Value");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_smoothstep_float")) {
        const HdMaterialConnectionVectorContainerSchema connections =
            nodeSchema.GetInputConnections();
        if (connections.Get(TfToken("low")).GetNumElements() != 0 ||
            connections.Get(TfToken("high")).GetNumElements() != 0)
        {
          TF_RUNTIME_ERROR("MaterialX smoothstep requires literal edges");
          continue;
        }
        const HdMaterialNodeParameterContainerSchema parameters = nodeSchema.GetParameters();
        const auto float_parameter_value = [&](const TfToken &name,
                                               const float default_value,
                                               float *value) {
          const HdSampledDataSourceHandle data = parameters.Get(name).GetValue();
          if (!data) {
            *value = default_value;
            return true;
          }
          const VtValue typed_value = data->GetValue(0.0f);
          if (!typed_value.IsHolding<float>()) {
            return false;
          }
          *value = typed_value.UncheckedGet<float>();
          return std::isfinite(*value);
        };
        float low;
        float high;
        if (!float_parameter_value(TfToken("low"), 0.0f, &low) ||
            !float_parameter_value(TfToken("high"), 1.0f, &high) || low >= high)
        {
          TF_RUNTIME_ERROR("MaterialX smoothstep requires finite low < high");
          continue;
        }
        MapRangeNode *range = graph->create_node<MapRangeNode>();
        range->set_range_type(NODE_MAP_RANGE_SMOOTHSTEP);
        range->set_clamp(false);
        range->set_to_min(0.0f);
        range->set_to_max(1.0f);
        nodeDesc.node = range;
        nodeDesc.input_endpoints[TfToken("in")] = {range->input("Value")};
        nodeDesc.input_endpoints[TfToken("low")] = {range->input("From Min")};
        nodeDesc.input_endpoints[TfToken("high")] = {range->input("From Max")};
        nodeDesc.output_endpoints[TfToken("out")] = range->output("Result");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, parameters, nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_smoothstep_vector2") ||
          nodeTypeIdToken == TfToken("ND_smoothstep_vector2FA") ||
          nodeTypeIdToken == TfToken("ND_smoothstep_vector3") ||
          nodeTypeIdToken == TfToken("ND_smoothstep_vector3FA"))
      {
        const HdMaterialConnectionVectorContainerSchema connections =
            nodeSchema.GetInputConnections();
        if (connections.Get(TfToken("low")).GetNumElements() != 0 ||
            connections.Get(TfToken("high")).GetNumElements() != 0)
        {
          TF_RUNTIME_ERROR("MaterialX smoothstep requires literal edges");
          continue;
        }
        const bool is_vector2 = nodeTypeIdToken == TfToken("ND_smoothstep_vector2") ||
                                nodeTypeIdToken == TfToken("ND_smoothstep_vector2FA");
        const bool scalar_edges = nodeTypeIdToken == TfToken("ND_smoothstep_vector2FA") ||
                                  nodeTypeIdToken == TfToken("ND_smoothstep_vector3FA");
        const HdMaterialNodeParameterContainerSchema parameters = nodeSchema.GetParameters();
        bool valid_edges = false;
        if (scalar_edges) {
          const auto float_value = [&](const TfToken &name,
                                       const float default_value,
                                       float *value) {
            const HdSampledDataSourceHandle data = parameters.Get(name).GetValue();
            if (!data) {
              *value = default_value;
              return true;
            }
            const VtValue typed_value = data->GetValue(0.0f);
            if (!typed_value.IsHolding<float>()) {
              return false;
            }
            *value = typed_value.UncheckedGet<float>();
            return std::isfinite(*value);
          };
          float low;
          float high;
          valid_edges = float_value(TfToken("low"), 0.0f, &low) &&
                        float_value(TfToken("high"), 1.0f, &high) && low < high;
        }
        else if (is_vector2) {
          const auto vector_value = [&](const TfToken &name,
                                        const pxr::GfVec2f &default_value,
                                        pxr::GfVec2f *value) {
            const HdSampledDataSourceHandle data = parameters.Get(name).GetValue();
            if (!data) {
              *value = default_value;
              return true;
            }
            const VtValue typed_value = data->GetValue(0.0f);
            if (!typed_value.IsHolding<pxr::GfVec2f>()) {
              return false;
            }
            *value = typed_value.UncheckedGet<pxr::GfVec2f>();
            return std::isfinite((*value)[0]) && std::isfinite((*value)[1]);
          };
          pxr::GfVec2f low;
          pxr::GfVec2f high;
          valid_edges = vector_value(TfToken("low"), pxr::GfVec2f(0.0f), &low) &&
                        vector_value(TfToken("high"), pxr::GfVec2f(1.0f), &high) &&
                        low[0] < high[0] && low[1] < high[1];
        }
        else {
          const auto vector_value = [&](const TfToken &name,
                                        const pxr::GfVec3f &default_value,
                                        pxr::GfVec3f *value) {
            const HdSampledDataSourceHandle data = parameters.Get(name).GetValue();
            if (!data) {
              *value = default_value;
              return true;
            }
            const VtValue typed_value = data->GetValue(0.0f);
            if (!typed_value.IsHolding<pxr::GfVec3f>()) {
              return false;
            }
            *value = typed_value.UncheckedGet<pxr::GfVec3f>();
            return std::isfinite((*value)[0]) && std::isfinite((*value)[1]) &&
                   std::isfinite((*value)[2]);
          };
          pxr::GfVec3f low;
          pxr::GfVec3f high;
          valid_edges = vector_value(TfToken("low"), pxr::GfVec3f(0.0f), &low) &&
                        vector_value(TfToken("high"), pxr::GfVec3f(1.0f), &high) &&
                        low[0] < high[0] && low[1] < high[1] && low[2] < high[2];
        }
        if (!valid_edges) {
          TF_RUNTIME_ERROR("MaterialX smoothstep requires finite low < high");
          continue;
        }

        SeparateXYZNode *input = graph->create_node<SeparateXYZNode>();
        SeparateXYZNode *low = scalar_edges ? nullptr : graph->create_node<SeparateXYZNode>();
        SeparateXYZNode *high = scalar_edges ? nullptr : graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        if (is_vector2) {
          combine->set_z(0.0f);
        }
        const std::initializer_list<const char *> channels =
            is_vector2 ? std::initializer_list<const char *>{"X", "Y"} :
                         std::initializer_list<const char *>{"X", "Y", "Z"};
        std::vector<ShaderInput *> low_inputs;
        std::vector<ShaderInput *> high_inputs;
        for (const char *channel : channels) {
          MathNode *numerator = graph->create_node<MathNode>();
          numerator->set_math_type(NODE_MATH_SUBTRACT);
          MathNode *denominator = graph->create_node<MathNode>();
          denominator->set_math_type(NODE_MATH_SUBTRACT);
          MathNode *divide = graph->create_node<MathNode>();
          divide->set_math_type(NODE_MATH_DIVIDE);
          MathNode *maximum = graph->create_node<MathNode>();
          maximum->set_math_type(NODE_MATH_MAXIMUM);
          maximum->set_value2(0.0f);
          MathNode *minimum = graph->create_node<MathNode>();
          minimum->set_math_type(NODE_MATH_MINIMUM);
          minimum->set_value2(1.0f);
          MathNode *square = graph->create_node<MathNode>();
          square->set_math_type(NODE_MATH_MULTIPLY);
          MathNode *twice = graph->create_node<MathNode>();
          twice->set_math_type(NODE_MATH_MULTIPLY);
          twice->set_value2(2.0f);
          MathNode *cubic = graph->create_node<MathNode>();
          cubic->set_math_type(NODE_MATH_SUBTRACT);
          cubic->set_value1(3.0f);
          MathNode *result = graph->create_node<MathNode>();
          result->set_math_type(NODE_MATH_MULTIPLY);

          graph->connect(input->output(channel), numerator->input("Value1"));
          graph->connect(numerator->output("Value"), divide->input("Value1"));
          graph->connect(denominator->output("Value"), divide->input("Value2"));
          graph->connect(divide->output("Value"), maximum->input("Value1"));
          graph->connect(maximum->output("Value"), minimum->input("Value1"));
          graph->connect(minimum->output("Value"), square->input("Value1"));
          graph->connect(minimum->output("Value"), square->input("Value2"));
          graph->connect(minimum->output("Value"), twice->input("Value1"));
          graph->connect(twice->output("Value"), cubic->input("Value2"));
          graph->connect(square->output("Value"), result->input("Value1"));
          graph->connect(cubic->output("Value"), result->input("Value2"));
          graph->connect(result->output("Value"), combine->input(channel));

          if (scalar_edges) {
            low_inputs.push_back(numerator->input("Value2"));
            low_inputs.push_back(denominator->input("Value2"));
            high_inputs.push_back(denominator->input("Value1"));
          }
          else {
            graph->connect(low->output(channel), numerator->input("Value2"));
            graph->connect(low->output(channel), denominator->input("Value2"));
            graph->connect(high->output(channel), denominator->input("Value1"));
          }
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {input->input("Vector")};
        nodeDesc.input_endpoints[TfToken("low")] =
            scalar_edges ? low_inputs : std::vector<ShaderInput *>{low->input("Vector")};
        nodeDesc.input_endpoints[TfToken("high")] =
            scalar_edges ? high_inputs : std::vector<ShaderInput *>{high->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, parameters, nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_remap_vector2") ||
          nodeTypeIdToken == TfToken("ND_remap_vector3") ||
          nodeTypeIdToken == TfToken("ND_remap_vector2FA") ||
          nodeTypeIdToken == TfToken("ND_remap_vector3FA"))
      {
        const bool is_vector2 = nodeTypeIdToken == TfToken("ND_remap_vector2") ||
                                nodeTypeIdToken == TfToken("ND_remap_vector2FA");
        const bool scalar = nodeTypeIdToken == TfToken("ND_remap_vector2FA") ||
                            nodeTypeIdToken == TfToken("ND_remap_vector3FA");
        SeparateXYZNode *in = graph->create_node<SeparateXYZNode>();
        std::array<SeparateXYZNode *, 4> values = {};
        if (!scalar) for (SeparateXYZNode *&value : values) value = graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        if (is_vector2) combine->set_z(0.0f);
        const std::initializer_list<const char *> channels = is_vector2 ? std::initializer_list<const char *>{"X", "Y"} : std::initializer_list<const char *>{"X", "Y", "Z"};
        std::array<std::vector<ShaderInput *>, 4> scalar_inputs;
        for (const char *channel : channels) {
          MathNode *sub_in = graph->create_node<MathNode>(); sub_in->set_math_type(NODE_MATH_SUBTRACT);
          MathNode *sub_out = graph->create_node<MathNode>(); sub_out->set_math_type(NODE_MATH_SUBTRACT);
          MathNode *mul = graph->create_node<MathNode>(); mul->set_math_type(NODE_MATH_MULTIPLY);
          MathNode *sub_range = graph->create_node<MathNode>(); sub_range->set_math_type(NODE_MATH_SUBTRACT);
          MathNode *div = graph->create_node<MathNode>(); div->set_math_type(NODE_MATH_DIVIDE);
          MathNode *add = graph->create_node<MathNode>(); add->set_math_type(NODE_MATH_ADD);
          graph->connect(in->output(channel), sub_in->input("Value1"));
          if (scalar) { scalar_inputs[0].push_back(sub_in->input("Value2")); scalar_inputs[0].push_back(sub_range->input("Value2")); scalar_inputs[1].push_back(sub_range->input("Value1")); scalar_inputs[2].push_back(sub_out->input("Value1")); scalar_inputs[2].push_back(add->input("Value2")); scalar_inputs[3].push_back(sub_out->input("Value2")); }
          else { graph->connect(values[0]->output(channel), sub_in->input("Value2")); graph->connect(values[1]->output(channel), sub_range->input("Value1")); graph->connect(values[0]->output(channel), sub_range->input("Value2")); graph->connect(values[2]->output(channel), sub_out->input("Value1")); graph->connect(values[3]->output(channel), sub_out->input("Value2")); graph->connect(values[2]->output(channel), add->input("Value2")); }
          graph->connect(sub_in->output("Value"), mul->input("Value1")); graph->connect(sub_out->output("Value"), mul->input("Value2")); graph->connect(sub_range->output("Value"), div->input("Value2")); graph->connect(mul->output("Value"), div->input("Value1")); graph->connect(div->output("Value"), add->input("Value1")); graph->connect(add->output("Value"), combine->input(channel));
        }
        nodeDesc.node = combine; nodeDesc.input_endpoints[TfToken("in")] = {in->input("Vector")};
        const std::array<TfToken,4> names = {TfToken("inlow"),TfToken("inhigh"),TfToken("outlow"),TfToken("outhigh")};
        for (size_t i=0;i<names.size();i++) nodeDesc.input_endpoints[names[i]] = scalar ? scalar_inputs[i] : std::vector<ShaderInput *>{values[i]->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector"); _nodes.emplace(nodePath,nodeDesc); UpdateParameters(nodeDesc,nodeSchema.GetParameters(),nodePath); continue;
      }

      if (nodeTypeIdToken == TfToken("ND_absval_color3") ||
          nodeTypeIdToken == TfToken("ND_ceil_color3") ||
          nodeTypeIdToken == TfToken("ND_floor_color3") ||
          nodeTypeIdToken == TfToken("ND_fract_color3") ||
          nodeTypeIdToken == TfToken("ND_round_color3") ||
          nodeTypeIdToken == TfToken("ND_sign_color3"))
      {
        const NodeMathType math_type =
            nodeTypeIdToken == TfToken("ND_absval_color3") ? NODE_MATH_ABSOLUTE :
            nodeTypeIdToken == TfToken("ND_ceil_color3") ? NODE_MATH_CEIL :
            nodeTypeIdToken == TfToken("ND_floor_color3") ? NODE_MATH_FLOOR :
            nodeTypeIdToken == TfToken("ND_fract_color3") ? NODE_MATH_FRACTION :
            nodeTypeIdToken == TfToken("ND_round_color3") ? NODE_MATH_ROUND :
                                                            NODE_MATH_SIGN;
        SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
        separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
        CombineColorNode *combine = graph->create_node<CombineColorNode>();
        combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
        for (const auto &[color, input] : {std::pair{"Red", "Red"},
                                           std::pair{"Green", "Green"},
                                           std::pair{"Blue", "Blue"}}) {
          MathNode *math = graph->create_node<MathNode>();
          math->set_math_type(math_type);
          graph->connect(separate->output(color), math->input("Value1"));
          graph->connect(math->output("Value"), combine->input(input));
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Color")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Color");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_asin_vector2") ||
          nodeTypeIdToken == TfToken("ND_acos_vector2") ||
          nodeTypeIdToken == TfToken("ND_sqrt_vector2") ||
          nodeTypeIdToken == TfToken("ND_ln_vector2") ||
          nodeTypeIdToken == TfToken("ND_exp_vector2") ||
          nodeTypeIdToken == TfToken("ND_round_vector2") ||
          nodeTypeIdToken == TfToken("ND_asin_vector3") ||
          nodeTypeIdToken == TfToken("ND_acos_vector3") ||
          nodeTypeIdToken == TfToken("ND_sqrt_vector3") ||
          nodeTypeIdToken == TfToken("ND_ln_vector3") ||
          nodeTypeIdToken == TfToken("ND_exp_vector3") ||
          nodeTypeIdToken == TfToken("ND_round_vector3"))
      {
        /* Cycles has no matching component-wise Vector Math operations for
         * these MaterialX nodes. Keep the declared Vector2/Vector3 arity by
         * lowering each defined component to scalar Math. */
        const bool is_vector2 = nodeTypeIdToken == TfToken("ND_asin_vector2") ||
                                nodeTypeIdToken == TfToken("ND_acos_vector2") ||
                                nodeTypeIdToken == TfToken("ND_sqrt_vector2") ||
                                nodeTypeIdToken == TfToken("ND_ln_vector2") ||
                                nodeTypeIdToken == TfToken("ND_exp_vector2") ||
                                nodeTypeIdToken == TfToken("ND_round_vector2");
        const NodeMathType math_type =
            (nodeTypeIdToken == TfToken("ND_asin_vector2") ||
             nodeTypeIdToken == TfToken("ND_asin_vector3")) ?
                NODE_MATH_ARCSINE :
            (nodeTypeIdToken == TfToken("ND_acos_vector2") ||
             nodeTypeIdToken == TfToken("ND_acos_vector3")) ?
                NODE_MATH_ARCCOSINE :
            (nodeTypeIdToken == TfToken("ND_sqrt_vector2") ||
             nodeTypeIdToken == TfToken("ND_sqrt_vector3")) ?
                NODE_MATH_SQRT :
            (nodeTypeIdToken == TfToken("ND_ln_vector2") ||
             nodeTypeIdToken == TfToken("ND_ln_vector3")) ?
                NODE_MATH_LOGARITHM :
            (nodeTypeIdToken == TfToken("ND_exp_vector2") ||
             nodeTypeIdToken == TfToken("ND_exp_vector3")) ?
                NODE_MATH_EXPONENT :
                NODE_MATH_ROUND;
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        if (is_vector2) {
          combine->set_z(0.0f);
        }
        const std::initializer_list<const char *> channels = is_vector2 ?
                                                           std::initializer_list<const char *>{"X", "Y"} :
                                                           std::initializer_list<const char *>{"X", "Y", "Z"};
        for (const char *channel : channels) {
          MathNode *math = graph->create_node<MathNode>();
          math->set_math_type(math_type);
          if (math_type == NODE_MATH_LOGARITHM) {
            /* Cycles logarithm is log(Value1, Value2); MaterialX ln is base e. */
            math->set_value2(2.71828182845904523536f);
          }
          graph->connect(separate->output(channel), math->input("Value1"));
          graph->connect(math->output("Value"), combine->input(channel));
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_atan2_vector2") ||
          nodeTypeIdToken == TfToken("ND_atan2_vector3"))
      {
        /* MaterialX atan2 is component-wise atan2(iny, inx). Cycles only
         * offers atan2 as scalar math, so retain the declared component count
         * explicitly instead of treating Vector2 as a three-component value. */
        const bool is_vector2 = nodeTypeIdToken == TfToken("ND_atan2_vector2");
        SeparateXYZNode *separate_y = graph->create_node<SeparateXYZNode>();
        SeparateXYZNode *separate_x = graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        if (is_vector2) {
          combine->set_z(0.0f);
        }
        const std::initializer_list<const char *> channels = is_vector2 ?
                                                           std::initializer_list<const char *>{"X", "Y"} :
                                                           std::initializer_list<const char *>{"X", "Y", "Z"};
        for (const char *channel : channels) {
          MathNode *math = graph->create_node<MathNode>();
          math->set_math_type(NODE_MATH_ARCTAN2);
          graph->connect(separate_y->output(channel), math->input("Value1"));
          graph->connect(separate_x->output(channel), math->input("Value2"));
          graph->connect(math->output("Value"), combine->input(channel));
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("iny")] = {separate_y->input("Vector")};
        nodeDesc.input_endpoints[TfToken("inx")] = {separate_x->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_clamp_vector2FA")) {
        /* Keep Vector2 scalar bounds to the two declared components so a
         * scalar broadcast cannot fabricate a meaningful Z value. */
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        combine->set_z(0.0f);
        std::vector<ShaderInput *> low_inputs;
        std::vector<ShaderInput *> high_inputs;
        for (const char *channel : {"X", "Y"}) {
          MathNode *maximum = graph->create_node<MathNode>();
          maximum->set_math_type(NODE_MATH_MAXIMUM);
          MathNode *minimum = graph->create_node<MathNode>();
          minimum->set_math_type(NODE_MATH_MINIMUM);
          graph->connect(separate->output(channel), maximum->input("Value1"));
          graph->connect(maximum->output("Value"), minimum->input("Value1"));
          graph->connect(minimum->output("Value"), combine->input(channel));
          low_inputs.push_back(maximum->input("Value2"));
          high_inputs.push_back(minimum->input("Value2"));
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Vector")};
        nodeDesc.input_endpoints[TfToken("low")] = low_inputs;
        nodeDesc.input_endpoints[TfToken("high")] = high_inputs;
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_clamp_vector2") ||
          nodeTypeIdToken == TfToken("ND_clamp_vector3") ||
          nodeTypeIdToken == TfToken("ND_clamp_vector3FA"))
      {
        /* MaterialX clamp is component-wise min(max(in, low), high). */
        const bool has_float_bounds = nodeTypeIdToken == TfToken("ND_clamp_vector3FA");
        VectorMathNode *maximum = graph->create_node<VectorMathNode>();
        maximum->set_math_type(NODE_VECTOR_MATH_MAXIMUM);
        VectorMathNode *minimum = graph->create_node<VectorMathNode>();
        minimum->set_math_type(NODE_VECTOR_MATH_MINIMUM);
        graph->connect(maximum->output("Vector"), minimum->input("Vector1"));

        nodeDesc.node = minimum;
        nodeDesc.input_endpoints[TfToken("in")] = {maximum->input("Vector1")};
        if (has_float_bounds) {
          CombineXYZNode *low = graph->create_node<CombineXYZNode>();
          CombineXYZNode *high = graph->create_node<CombineXYZNode>();
          graph->connect(low->output("Vector"), maximum->input("Vector2"));
          graph->connect(high->output("Vector"), minimum->input("Vector2"));
          nodeDesc.input_endpoints[TfToken("low")] = {
              low->input("X"), low->input("Y"), low->input("Z")};
          nodeDesc.input_endpoints[TfToken("high")] = {
              high->input("X"), high->input("Y"), high->input("Z")};
        }
        else {
          nodeDesc.input_endpoints[TfToken("low")] = {maximum->input("Vector2")};
          nodeDesc.input_endpoints[TfToken("high")] = {minimum->input("Vector2")};
        }
        nodeDesc.output_endpoints[TfToken("out")] = minimum->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_invert_vector2") ||
          nodeTypeIdToken == TfToken("ND_invert_vector3"))
      {
        /* MaterialX invert is amount - in, not a blend operation. */
        VectorMathNode *subtract = graph->create_node<VectorMathNode>();
        subtract->set_math_type(NODE_VECTOR_MATH_SUBTRACT);
        nodeDesc.node = subtract;
        nodeDesc.input_endpoints[TfToken("amount")] = {subtract->input("Vector1")};
        nodeDesc.input_endpoints[TfToken("in")] = {subtract->input("Vector2")};
        nodeDesc.output_endpoints[TfToken("out")] = subtract->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_invert_vector2FA") ||
          nodeTypeIdToken == TfToken("ND_invert_vector3FA"))
      {
        /* Broadcast the scalar amount only to the declared Vector2/Vector3
         * components, retaining the MaterialX expression amount - in. */
        const bool is_vector2 = nodeTypeIdToken == TfToken("ND_invert_vector2FA");
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        if (is_vector2) {
          combine->set_z(0.0f);
        }
        const std::initializer_list<const char *> channels = is_vector2 ?
                                                           std::initializer_list<const char *>{"X", "Y"} :
                                                           std::initializer_list<const char *>{"X", "Y", "Z"};
        std::vector<ShaderInput *> amount_inputs;
        for (const char *channel : channels) {
          MathNode *subtract = graph->create_node<MathNode>();
          subtract->set_math_type(NODE_MATH_SUBTRACT);
          graph->connect(separate->output(channel), subtract->input("Value2"));
          graph->connect(subtract->output("Value"), combine->input(channel));
          amount_inputs.push_back(subtract->input("Value1"));
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("amount")] = amount_inputs;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_min_vector2FA") ||
          nodeTypeIdToken == TfToken("ND_min_vector3FA") ||
          nodeTypeIdToken == TfToken("ND_max_vector2FA") ||
          nodeTypeIdToken == TfToken("ND_max_vector3FA"))
      {
        const bool is_vector2 = nodeTypeIdToken == TfToken("ND_min_vector2FA") ||
                                nodeTypeIdToken == TfToken("ND_max_vector2FA");
        const NodeMathType math_type =
            (nodeTypeIdToken == TfToken("ND_min_vector2FA") ||
             nodeTypeIdToken == TfToken("ND_min_vector3FA")) ?
                NODE_MATH_MINIMUM :
                NODE_MATH_MAXIMUM;
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        if (is_vector2) {
          combine->set_z(0.0f);
        }
        const std::initializer_list<const char *> channels = is_vector2 ?
                                                           std::initializer_list<const char *>{"X", "Y"} :
                                                           std::initializer_list<const char *>{"X", "Y", "Z"};
        std::vector<ShaderInput *> scalar_in2_inputs;
        for (const char *channel : channels) {
          MathNode *math = graph->create_node<MathNode>();
          math->set_math_type(math_type);
          graph->connect(separate->output(channel), math->input("Value1"));
          graph->connect(math->output("Value"), combine->input(channel));
          scalar_in2_inputs.push_back(math->input("Value2"));
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in1")] = {separate->input("Vector")};
        nodeDesc.input_endpoints[TfToken("in2")] = scalar_in2_inputs;
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_modulo_vector2") ||
          nodeTypeIdToken == TfToken("ND_modulo_vector3") ||
          nodeTypeIdToken == TfToken("ND_modulo_vector2FA") ||
          nodeTypeIdToken == TfToken("ND_modulo_vector3FA") ||
          nodeTypeIdToken == TfToken("ND_power_vector2") ||
          nodeTypeIdToken == TfToken("ND_power_vector3") ||
          nodeTypeIdToken == TfToken("ND_power_vector2FA") ||
          nodeTypeIdToken == TfToken("ND_power_vector3FA"))
      {
        /* Preserve MaterialX in1/in2 component semantics with scalar Math.
         * FA forms broadcast their scalar in2 directly to every component. */
        const bool is_vector2 = nodeTypeIdToken == TfToken("ND_modulo_vector2") ||
                                nodeTypeIdToken == TfToken("ND_modulo_vector2FA") ||
                                nodeTypeIdToken == TfToken("ND_power_vector2") ||
                                nodeTypeIdToken == TfToken("ND_power_vector2FA");
        const bool has_scalar_in2 = nodeTypeIdToken == TfToken("ND_modulo_vector2FA") ||
                                   nodeTypeIdToken == TfToken("ND_modulo_vector3FA") ||
                                   nodeTypeIdToken == TfToken("ND_power_vector2FA") ||
                                   nodeTypeIdToken == TfToken("ND_power_vector3FA");
        const NodeMathType math_type =
            (nodeTypeIdToken == TfToken("ND_modulo_vector2") ||
             nodeTypeIdToken == TfToken("ND_modulo_vector3") ||
             nodeTypeIdToken == TfToken("ND_modulo_vector2FA") ||
             nodeTypeIdToken == TfToken("ND_modulo_vector3FA")) ?
                NODE_MATH_MODULO :
                NODE_MATH_POWER;
        SeparateXYZNode *separate1 = graph->create_node<SeparateXYZNode>();
        SeparateXYZNode *separate2 = has_scalar_in2 ? nullptr : graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        if (is_vector2) {
          combine->set_z(0.0f);
        }
        const std::initializer_list<const char *> channels = is_vector2 ?
                                                           std::initializer_list<const char *>{"X", "Y"} :
                                                           std::initializer_list<const char *>{"X", "Y", "Z"};
        std::vector<ShaderInput *> scalar_in2_inputs;
        for (const char *channel : channels) {
          MathNode *math = graph->create_node<MathNode>();
          math->set_math_type(math_type);
          graph->connect(separate1->output(channel), math->input("Value1"));
          if (has_scalar_in2) {
            scalar_in2_inputs.push_back(math->input("Value2"));
          }
          else {
            graph->connect(separate2->output(channel), math->input("Value2"));
          }
          graph->connect(math->output("Value"), combine->input(channel));
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in1")] = {separate1->input("Vector")};
        nodeDesc.input_endpoints[TfToken("in2")] = has_scalar_in2 ?
                                                          scalar_in2_inputs :
                                                          std::vector<ShaderInput *>{separate2->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_divide_vector3FA")) {
        /* MaterialX FA nodes broadcast their scalar second operand.  Cycles
         * Vector Math has two vector inputs, so preserve the scalar socket
         * contract by lowering the three declared components explicitly. */
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        std::vector<ShaderInput *> scalar_in2_inputs;
        for (const char *channel : {"X", "Y", "Z"}) {
          MathNode *math = graph->create_node<MathNode>();
          math->set_math_type(NODE_MATH_DIVIDE);
          graph->connect(separate->output(channel), math->input("Value1"));
          scalar_in2_inputs.push_back(math->input("Value2"));
          graph->connect(math->output("Value"), combine->input(channel));
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in1")] = {separate->input("Vector")};
        nodeDesc.input_endpoints[TfToken("in2")] = scalar_in2_inputs;
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_divide_vector2") ||
          nodeTypeIdToken == TfToken("ND_divide_vector2FA"))
      {
        /* A direct Vector Math divide would also evaluate the adapter's Z channel,
         * which is 0/0 for vector2 inputs.  MaterialX vector2 division only has
         * X/Y components, so lower those components explicitly and construct a
         * literal zero Z output. */
        const bool has_scalar_in2 = nodeTypeIdToken == TfToken("ND_divide_vector2FA");
        SeparateXYZNode *separate1 = graph->create_node<SeparateXYZNode>();
        SeparateXYZNode *separate2 = has_scalar_in2 ? nullptr : graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        combine->set_z(0.0f);
        std::vector<ShaderInput *> scalar_in2_inputs;
        for (const char *channel : {"X", "Y"}) {
          MathNode *math = graph->create_node<MathNode>();
          math->set_math_type(NODE_MATH_DIVIDE);
          graph->connect(separate1->output(channel), math->input("Value1"));
          if (has_scalar_in2) {
            scalar_in2_inputs.push_back(math->input("Value2"));
          }
          else {
            graph->connect(separate2->output(channel), math->input("Value2"));
          }
          graph->connect(math->output("Value"), combine->input(channel));
        }
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in1")] = {separate1->input("Vector")};
        nodeDesc.input_endpoints[TfToken("in2")] = has_scalar_in2 ?
                                                          scalar_in2_inputs :
                                                          std::vector<ShaderInput *>{separate2->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_float_vector3")) {
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {
            combine->input("X"), combine->input("Y"), combine->input("Z")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_integer_vector3")) {
        if (nodeSchema.GetInputConnections().Get(TfToken("in")).GetNumElements() != 0) {
          TF_RUNTIME_ERROR("MaterialX integer conversion does not support linked input");
          continue;
        }
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {
            combine->input("X"), combine->input("Y"), combine->input("Z")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_combine3_color3")) {
        CombineColorNode *combine = graph->create_node<CombineColorNode>();
        combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in1")] = {combine->input("Red")};
        nodeDesc.input_endpoints[TfToken("in2")] = {combine->input("Green")};
        nodeDesc.input_endpoints[TfToken("in3")] = {combine->input("Blue")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Color");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_separate3_color3")) {
        SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
        separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
        nodeDesc.node = separate;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Color")};
        nodeDesc.output_endpoints[TfToken("out1")] = separate->output("Red");
        nodeDesc.output_endpoints[TfToken("out2")] = separate->output("Green");
        nodeDesc.output_endpoints[TfToken("out3")] = separate->output("Blue");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_vector2_vector3")) {
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        combine->set_z(0.0f);
        graph->connect(separate->output("X"), combine->input("X"));
        graph->connect(separate->output("Y"), combine->input("Y"));

        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_convert_vector3_vector2")) {
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        combine->set_z(0.0f);
        graph->connect(separate->output("X"), combine->input("X"));
        graph->connect(separate->output("Y"), combine->input("Y"));
        nodeDesc.node = combine;
        nodeDesc.input_endpoints[TfToken("in")] = {separate->input("Vector")};
        nodeDesc.output_endpoints[TfToken("out")] = combine->output("Vector");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      if (nodeTypeIdToken == TfToken("ND_place2d_vector2")) {
        auto make_vector2 = [&](const float z) {
          SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
          CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
          combine->set_z(z);
          graph->connect(separate->output("X"), combine->input("X"));
          graph->connect(separate->output("Y"), combine->input("Y"));
          return std::make_pair(separate, combine);
        };
        const auto texcoord = make_vector2(0.0f);
        const auto pivot = make_vector2(0.0f);
        const auto scale = make_vector2(1.0f);
        scale.first->set_vector(make_float3(1.0f, 1.0f, 1.0f));
        const auto offset = make_vector2(0.0f);

        auto make_vector_math = [&](const NodeVectorMathType math_type) {
          VectorMathNode *node = graph->create_node<VectorMathNode>();
          node->set_math_type(math_type);
          return node;
        };
        VectorMathNode *subpivot = make_vector_math(NODE_VECTOR_MATH_SUBTRACT);
        VectorMathNode *applyscale = make_vector_math(NODE_VECTOR_MATH_DIVIDE);
        VectorMathNode *applyoffset = make_vector_math(NODE_VECTOR_MATH_SUBTRACT);
        VectorMathNode *applyoffset2 = make_vector_math(NODE_VECTOR_MATH_SUBTRACT);
        VectorMathNode *applyscale2 = make_vector_math(NODE_VECTOR_MATH_DIVIDE);
        VectorMathNode *addpivot = make_vector_math(NODE_VECTOR_MATH_ADD);
        VectorMathNode *addpivot2 = make_vector_math(NODE_VECTOR_MATH_ADD);
        MathNode *radians = graph->create_node<MathNode>();
        radians->set_math_type(NODE_MATH_RADIANS);
        VectorRotateNode *applyrot = graph->create_node<VectorRotateNode>();
        VectorRotateNode *applyrot2 = graph->create_node<VectorRotateNode>();
        for (VectorRotateNode *node : {applyrot, applyrot2}) {
          node->set_rotate_type(NODE_VECTOR_ROTATE_TYPE_AXIS_Z);
          node->set_invert(true);
          graph->connect(radians->output("Value"), node->input("Angle"));
        }
        MixVectorNode *operation_order = graph->create_node<MixVectorNode>();
        operation_order->set_fac(0.0f);

        graph->connect(texcoord.second->output("Vector"), subpivot->input("Vector1"));
        graph->connect(pivot.second->output("Vector"), subpivot->input("Vector2"));
        graph->connect(subpivot->output("Vector"), applyscale->input("Vector1"));
        graph->connect(scale.second->output("Vector"), applyscale->input("Vector2"));
        graph->connect(applyscale->output("Vector"), applyrot->input("Vector"));
        graph->connect(applyrot->output("Vector"), applyoffset->input("Vector1"));
        graph->connect(offset.second->output("Vector"), applyoffset->input("Vector2"));
        graph->connect(applyoffset->output("Vector"), addpivot->input("Vector1"));
        graph->connect(pivot.second->output("Vector"), addpivot->input("Vector2"));

        graph->connect(subpivot->output("Vector"), applyoffset2->input("Vector1"));
        graph->connect(offset.second->output("Vector"), applyoffset2->input("Vector2"));
        graph->connect(applyoffset2->output("Vector"), applyrot2->input("Vector"));
        graph->connect(applyrot2->output("Vector"), applyscale2->input("Vector1"));
        graph->connect(scale.second->output("Vector"), applyscale2->input("Vector2"));
        graph->connect(applyscale2->output("Vector"), addpivot2->input("Vector1"));
        graph->connect(pivot.second->output("Vector"), addpivot2->input("Vector2"));

        graph->connect(addpivot->output("Vector"), operation_order->input("A"));
        graph->connect(addpivot2->output("Vector"), operation_order->input("B"));

        nodeDesc.node = operation_order;
        nodeDesc.input_endpoints[TfToken("texcoord")] = {texcoord.first->input("Vector")};
        nodeDesc.input_endpoints[TfToken("pivot")] = {pivot.first->input("Vector")};
        nodeDesc.input_endpoints[TfToken("scale")] = {scale.first->input("Vector")};
        nodeDesc.input_endpoints[TfToken("rotate")] = {radians->input("Value1")};
        nodeDesc.input_endpoints[TfToken("offset")] = {offset.first->input("Vector")};
        nodeDesc.input_endpoints[TfToken("operationorder")] = {operation_order->input("Factor")};
        nodeDesc.output_endpoints[TfToken("out")] = operation_order->output("Result");
        _nodes.emplace(nodePath, nodeDesc);
        UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
        continue;
      }

      ustring cyclesType(nodeTypeId);
      if (nodeTypeId.starts_with("cycles_") || nodeTypeId.starts_with("cycles:")) {
        /* Native Cycles note embedded in USDShade. */
        cyclesType = nodeTypeId.substr(strlen("cycles_"));
        nodeDesc.mapping = sUsdToCyles->findCycles(cyclesType);
      }
      else {
        /* Check if any remapping is needed (e.g. for USDPreviewSurface to Cycles nodes). */
        nodeDesc.mapping = sUsdToCyles->findUsd(nodeTypeIdToken);
        if (nodeDesc.mapping) {
          cyclesType = nodeDesc.mapping->nodeType();
        }
      }

      /* If it's a native Cycles' node-type, just do the lookup now. */
      if (const NodeType *nodeType = NodeType::find(cyclesType)) {
        nodeDesc.node = graph->create_node(nodeType);
        if (nodeDesc.mapping) {
          nodeDesc.mapping->initializeNode(nodeDesc.node);
        }
        _nodes.emplace(nodePath, nodeDesc);
      }
      else {
        TF_RUNTIME_ERROR("Could not create node '%s'", nodePath.GetText());
        continue;
      }
    }

    UpdateParameters(nodeDesc, nodeSchema.GetParameters(), nodePath);
  }

  /* Now that all nodes have been constructed, iterate the network again and build up any
   * connections between nodes. */
  for (const TfToken &nodeName : nodes.GetNames()) {
    const SdfPath nodePath = MaterialNodeNameToSdfPath(nodeName);

    const auto nodeIt = _nodes.find(nodePath);
    if (nodeIt == _nodes.end()) {
      if (rejected_nodes.contains(nodePath)) {
        continue;
      }
      TF_RUNTIME_ERROR("Could not find node '%s' to connect", nodePath.GetText());
      continue;
    }

    UpdateConnections(nodeIt->second, nodes.Get(nodeName), nodePath, graph.get());
  }

  /* Finally connect the terminals to the graph output (Surface, Volume, Displacement). */
  HdMaterialConnectionContainerSchema terminals = network.GetTerminals();
  for (const TfToken &terminalName : terminals.GetNames()) {
    HdMaterialConnectionSchema termSchema = terminals.Get(terminalName);
    const SdfPath upstreamNodePath =
        termSchema.GetUpstreamNodePath() ?
            MaterialNodeNameToSdfPath(termSchema.GetUpstreamNodePath()->GetTypedValue(0.0f)) :
            SdfPath();
    const TfToken upstreamOutputName = termSchema.GetUpstreamNodeOutputName() ?
                                           termSchema.GetUpstreamNodeOutputName()->GetTypedValue(
                                               0.0f) :
                                           TfToken();

    const auto nodeIt = _nodes.find(upstreamNodePath);
    if (nodeIt == _nodes.end()) {
      TF_RUNTIME_ERROR("Could not find terminal node '%s'", upstreamNodePath.GetText());
      continue;
    }

    ShaderNode *const node = nodeIt->second.node;

    const char *inputName = nullptr;
    const char *outputName = nullptr;
    if (terminalName == HdMaterialTerminalTokens->surface ||
        terminalName == CyclesMaterialTokens->cyclesSurface)
    {
      inputName = "Surface";
      /* Find default output name based on the node if none is provided. */
      if (node->type->name == "add_closure" || node->type->name == "mix_closure") {
        outputName = "Closure";
      }
      else if (node->type->name == "emission") {
        outputName = "Emission";
      }
      else {
        outputName = "BSDF";
      }
    }
    else if (terminalName == HdMaterialTerminalTokens->displacement ||
             terminalName == CyclesMaterialTokens->cyclesDisplacement)
    {
      inputName = outputName = "Displacement";
    }
    else if (terminalName == HdMaterialTerminalTokens->volume ||
             terminalName == CyclesMaterialTokens->cyclesVolume)
    {
      inputName = outputName = "Volume";
    }

    /* For native Cycles nodes we use the upstream output name as is, for
     * mapping from e.g. UsdPreviewSurface we need to use the default output
     * name that is known to exist. */
    if (!upstreamOutputName.IsEmpty() && nodeIt->second.mapping == nullptr) {
      outputName = upstreamOutputName.GetText();
    }

    ShaderInput *const input = inputName ? graph->output()->input(inputName) : nullptr;
    if (!input) {
      TF_RUNTIME_ERROR("Could not find terminal input '%s.%s'",
                       upstreamNodePath.GetText(),
                       inputName ? inputName : "<null>");
      continue;
    }

    ShaderOutput *const output = outputName ? node->output(outputName) : nullptr;
    if (!output) {
      TF_RUNTIME_ERROR("Could not find terminal output '%s.%s'",
                       upstreamNodePath.GetText(),
                       outputName ? outputName : "<null>");
      continue;
    }

    graph->connect(output, input);
  }

  /* Create the instanceId AOV output. */
  {
    const ustring instanceId(HdAovTokens->instanceId.GetString());

    OutputAOVNode *aovNode = graph->create_node<OutputAOVNode>();
    aovNode->set_name(instanceId);

    AttributeNode *instanceIdNode = graph->create_node<AttributeNode>();
    instanceIdNode->set_attribute(instanceId);

    graph->connect(instanceIdNode->output("Fac"), aovNode->input("Value"));
  }

  _shader->set_graph(std::move(graph));
}

void HdCyclesMaterial::Finalize(HdRenderParam *renderParam)
{
  if (!_shader) {
    return;
  }

  const SceneLock lock(renderParam);
  const bool keep_nodes = static_cast<const HdCyclesSession *>(renderParam)->keep_nodes;

  _nodes.clear();

  if (!keep_nodes) {
    lock.scene->delete_node(_shader);
  }
  _shader = nullptr;
}

void HdCyclesMaterial::Initialize(HdRenderParam *renderParam)
{
  if (_shader) {
    return;
  }

  const SceneLock lock(renderParam);

  _shader = lock.scene->create_node<Shader>();
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
