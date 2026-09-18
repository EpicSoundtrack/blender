/* SPDX-FileCopyrightText: 2026 Blender Authors
 * SPDX-License-Identifier: Apache-2.0 */
#pragma once

/* Exact SDK-validated canonical fixtures from the public external .mtlx planner.
 * Each fixture was independently rejected by actual companion preflight before
 * this proposed fix. SHA256 is asserted by the test, without rewriting types. */
namespace materialx_vector3_fixtures {
struct Fixture {
  const char *name;
  const char *sha256;
  const char *usda;
};
static const Fixture fixtures[] = {
  {"convert_vector3_literal", "4c03dc1e99fcb0d41ced09d42ba0fc0c7faae04e24aa8aa6a294162cac57ee48",
    R"MXV3(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__convert" (
                    customData = {
                        string materialXNodeDef = "ND_convert_vector3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_convert_vector3_color3"
                    vector3f inputs:in = (0.2, 0.4, 0.6)
                    color3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root/Nested/root__convert.outputs:out>
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                       )MXV3"
    R"MXV3( }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                  )MXV3"
    R"MXV3(      }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                       )MXV3"
    R"MXV3(     string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)MXV3"},
  {"constant_vector3_to_convert", "38b7fa9b8c6be8be872519648d1f185ee13e05199e303b6983facdfdab327c65",
    R"MXV3(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__constant" (
                    customData = {
                        string materialXNodeDef = "ND_constant_vector3"
                    }
                )
                {
                    uniform token info:id = "ND_constant_vector3"
                    vector3f inputs:value = (0.2, 0.4, 0.6)
                    vector3f outputs:out
                }

                def Shader "root__convert" (
                    customData = {
                        string materialXNodeDef = "ND_convert_vector3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_convert_vector3_color3"
                    vector3f inputs:in.connect = </Looks/Material/Root/Nested/root__constant.outputs:out>
                    color3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root/Nested/root__convert.outputs:out>
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
       )MXV3"
    R"MXV3(                     string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
           )MXV3"
    R"MXV3(             }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
           )MXV3"
    R"MXV3(                 string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)MXV3"},
  {"normalmap_literal", "ce4eb7455d73e51342fc5fa835a4f34ebd727331989dcfb686d777f324eba03e",
    R"MXV3(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__normalmap" (
                    customData = {
                        string materialXNodeDef = "ND_normalmap_float"
                    }
                )
                {
                    uniform token info:id = "ND_normalmap_float"
                    vector3f inputs:bitangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Bworld"
                        }
                    )
                    vector3f inputs:in = (0.5, 0.5, 1)
                    vector3f inputs:normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                        }
                    )
                    float inputs:scale = 1
                    vector3f inputs:tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                        }
                    )
                    vector3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                          )MXV3"
    R"MXV3(  string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    vector3f inputs:geometry_normal.connect = </Looks/Material/Root/Nested/root__normalmap.outputs:out>
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            strin)MXV3"
    R"MXV3(g uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
)MXV3"
    R"MXV3(                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)MXV3"},
  {"normalmap_constant_vector3", "bfc77aada58619e1ff8c8c0c41b977040166516b684166e917fb7c9e0a6b3d5f",
    R"MXV3(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__constant" (
                    customData = {
                        string materialXNodeDef = "ND_constant_vector3"
                    }
                )
                {
                    uniform token info:id = "ND_constant_vector3"
                    vector3f inputs:value = (0.5, 0.5, 1)
                    vector3f outputs:out
                }

                def Shader "root__normalmap" (
                    customData = {
                        string materialXNodeDef = "ND_normalmap_float"
                    }
                )
                {
                    uniform token info:id = "ND_normalmap_float"
                    vector3f inputs:bitangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Bworld"
                        }
                    )
                    vector3f inputs:in.connect = </Looks/Material/Root/Nested/root__constant.outputs:out>
                    vector3f inputs:normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                        }
                    )
                    float inputs:scale = 1
                    vector3f inputs:tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                        }
                    )
                    vector3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadv)MXV3"
    R"MXV3(anced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    vector3f inputs:geometry_normal.connect = </Looks/Material/Root/Nested/root__normalmap.outputs:out>
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
      )MXV3"
    R"MXV3(                      string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                       )MXV3"
    R"MXV3( }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)MXV3"},
};
}  // namespace materialx_vector3_fixtures

namespace materialx_vector3_fixtures {
static const Fixture remaining_fixtures[] = {
  {"normalize", "505986840e15aa704b243c49468c4b2897e94af9b38449cde89fb1457c37a966",
    R"MXV3(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__vector_op" (
                    customData = {
                        string materialXNodeDef = "ND_normalize_vector3"
                    }
                )
                {
                    uniform token info:id = "ND_normalize_vector3"
                    vector3f inputs:in = (0.2, 0.4, 0.6)
                    vector3f outputs:out
                }

                def Shader "root__convert" (
                    customData = {
                        string materialXNodeDef = "ND_convert_vector3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_convert_vector3_color3"
                    vector3f inputs:in.connect = </Looks/Material/Root/Nested/root__vector_op.outputs:out>
                    color3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root/Nested/root__convert.outputs:out>
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
      )MXV3"
    R"MXV3(                      string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
          )MXV3"
    R"MXV3(              }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
          )MXV3"
    R"MXV3(                  string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)MXV3"
  },
  {"clamp", "43f0e7d9c13cc68155e1ddba101fd57bc83b57249fedf3bf5bfa9127fb8fb9c6",
    R"MXV3(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__vector_op" (
                    customData = {
                        string materialXNodeDef = "ND_clamp_vector3"
                    }
                )
                {
                    uniform token info:id = "ND_clamp_vector3"
                    vector3f inputs:high = (1, 1, 1)
                    vector3f inputs:in = (0.2, 0.4, 0.6)
                    vector3f inputs:low = (0, 0, 0)
                    vector3f outputs:out
                }

                def Shader "root__convert" (
                    customData = {
                        string materialXNodeDef = "ND_convert_vector3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_convert_vector3_color3"
                    vector3f inputs:in.connect = </Looks/Material/Root/Nested/root__vector_op.outputs:out>
                    color3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root/Nested/root__convert.outputs:out>
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
       )MXV3"
    R"MXV3(                     string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            )MXV3"
    R"MXV3(string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
        )MXV3"
    R"MXV3(                sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)MXV3"
  },
  {"ifgreater", "b05133b6b98e33c6511509befda440d4430e3becb9a1a5921eb7ecb0104beb13",
    R"MXV3(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__vector_op" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreater_vector3"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreater_vector3"
                    vector3f inputs:in1 = (0.2, 0.4, 0.6)
                    vector3f inputs:in2 = (0.6, 0.4, 0.2)
                    float inputs:value1 = 1
                    float inputs:value2 = 0
                    vector3f outputs:out
                }

                def Shader "root__convert" (
                    customData = {
                        string materialXNodeDef = "ND_convert_vector3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_convert_vector3_color3"
                    vector3f inputs:in.connect = </Looks/Material/Root/Nested/root__vector_op.outputs:out>
                    color3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root/Nested/root__convert.outputs:out>
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
          )MXV3"
    R"MXV3(                  string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                           )MXV3"
    R"MXV3( string uimin = "0.0"
                            string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inp)MXV3"
    R"MXV3(uts:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)MXV3"
  },
};
}  // namespace materialx_vector3_fixtures

/* Exact retained SDK canonical bytes for common vector3 families. */
namespace viewport_vector3_fixtures {
struct Fixture { const char *name; const char *nodedef; const char *sha256; const char *usda; };
static const Fixture fixtures[] = {
  {"add", "ND_add_vector3", "e7771c16077a1315cbbe59ac3b008e1c291c5f916b817b87a64f6a0654076ea8",
    R"VPAUDIT(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__vector_op" (
                    customData = {
                        string materialXNodeDef = "ND_add_vector3"
                    }
                )
                {
                    uniform token info:id = "ND_add_vector3"
                    vector3f inputs:in1 = (0.2, 0.4, 0.6)
                    vector3f inputs:in2 = (0.6, 0.4, 0.2)
                    vector3f outputs:out
                }

                def Shader "root__convert" (
                    customData = {
                        string materialXNodeDef = "ND_convert_vector3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_convert_vector3_color3"
                    vector3f inputs:in.connect = </Looks/Material/Root/Nested/root__vector_op.outputs:out>
                    color3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root/Nested/root__convert.outputs:out>
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
        )VPAUDIT"
    R"VPAUDIT(                    string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                )VPAUDIT"
    R"VPAUDIT(            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                  )VPAUDIT"
    R"VPAUDIT(          string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)VPAUDIT"
  },
  {"add_connected", "ND_add_vector3", "8572525c423978dc94044bbd9ef7694c4bca3f9eb0fe6224147d8dd9eab8adda",
    R"VPAUDIT(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__constant" (
                    customData = {
                        string materialXNodeDef = "ND_constant_vector3"
                    }
                )
                {
                    uniform token info:id = "ND_constant_vector3"
                    vector3f inputs:value = (0.2, 0.4, 0.6)
                    vector3f outputs:out
                }

                def Shader "root__vector_op" (
                    customData = {
                        string materialXNodeDef = "ND_add_vector3"
                    }
                )
                {
                    uniform token info:id = "ND_add_vector3"
                    vector3f inputs:in1.connect = </Looks/Material/Root/Nested/root__constant.outputs:out>
                    vector3f inputs:in2 = (0.6, 0.4, 0.2)
                    vector3f outputs:out
                }

                def Shader "root__convert" (
                    customData = {
                        string materialXNodeDef = "ND_convert_vector3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_convert_vector3_color3"
                    vector3f inputs:in.connect = </Looks/Material/Root/Nested/root__vector_op.outputs:out>
                    color3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root/Nested/root__convert.outputs:out>
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                )VPAUDIT"
    R"VPAUDIT(            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1)VPAUDIT"
    R"VPAUDIT(.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                )VPAUDIT"
    R"VPAUDIT(            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)VPAUDIT"
  },
  {"multiply_scalar", "ND_multiply_vector3FA", "9971fe4f46b006d9947ee677843b9a859de2f9af78cc94c9c76d41e11544dcae",
    R"VPAUDIT(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__vector_op" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_vector3FA"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_vector3FA"
                    vector3f inputs:in1 = (0.2, 0.4, 0.6)
                    float inputs:in2 = 0.5
                    vector3f outputs:out
                }

                def Shader "root__convert" (
                    customData = {
                        string materialXNodeDef = "ND_convert_vector3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_convert_vector3_color3"
                    vector3f inputs:in.connect = </Looks/Material/Root/Nested/root__vector_op.outputs:out>
                    color3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root/Nested/root__convert.outputs:out>
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
         )VPAUDIT"
    R"VPAUDIT(                   string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                 )VPAUDIT"
    R"VPAUDIT(           string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                   )VPAUDIT"
    R"VPAUDIT(         string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)VPAUDIT"
  },
  {"mix_scalar", "ND_mix_vector3", "6004cb8d8192e1c375da4a75514528f2972a779756c969abc526169dbcad313f",
    R"VPAUDIT(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__vector_op" (
                    customData = {
                        string materialXNodeDef = "ND_mix_vector3"
                    }
                )
                {
                    uniform token info:id = "ND_mix_vector3"
                    vector3f inputs:bg = (0.2, 0.4, 0.6)
                    vector3f inputs:fg = (0.6, 0.4, 0.2)
                    float inputs:mix = 0.5 (
                        sdrMetadata = {
                            string uisoftmax = "1.0"
                            string uisoftmin = "0.0"
                        }
                    )
                    vector3f outputs:out
                }

                def Shader "root__convert" (
                    customData = {
                        string materialXNodeDef = "ND_convert_vector3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_convert_vector3_color3"
                    vector3f inputs:in.connect = </Looks/Material/Root/Nested/root__vector_op.outputs:out>
                    color3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root/Nested/root__convert.outputs:out>
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
            )VPAUDIT"
    R"VPAUDIT(            }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_)VPAUDIT"
    R"VPAUDIT(weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
       )VPAUDIT"
    R"VPAUDIT(                     string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)VPAUDIT"
  },
  {"mix_vector", "ND_mix_vector3_vector3", "5da8a3190c213b3e7fa3ec818703eb226e164a11c114c891c07f864e0f6620a9",
    R"VPAUDIT(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface (
                customData = {
                    string materialXType = "surfaceshader"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:root__AuditMaterial_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:root__AuditMaterial_surfaceshader.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/root__surface.outputs:out>

                def Shader "root__vector_op" (
                    customData = {
                        string materialXNodeDef = "ND_mix_vector3_vector3"
                    }
                )
                {
                    uniform token info:id = "ND_mix_vector3_vector3"
                    vector3f inputs:bg = (0.2, 0.4, 0.6)
                    vector3f inputs:fg = (0.6, 0.4, 0.2)
                    vector3f inputs:mix = (0.25, 0.5, 0.75) (
                        sdrMetadata = {
                            string uisoftmax = "1,1,1"
                            string uisoftmin = "0,0,0"
                        }
                    )
                    vector3f outputs:out
                }

                def Shader "root__convert" (
                    customData = {
                        string materialXNodeDef = "ND_convert_vector3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_convert_vector3_color3"
                    vector3f inputs:in.connect = </Looks/Material/Root/Nested/root__vector_op.outputs:out>
                    color3f outputs:out
                }

                def Shader "root__surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root/Nested/root__convert.outputs:out>
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string ui)VPAUDIT"
    R"VPAUDIT(name = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
     )VPAUDIT"
    R"VPAUDIT(               float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                     )VPAUDIT"
    R"VPAUDIT(       string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)VPAUDIT"
  },
};
} // namespace viewport_vector3_fixtures
