/* SPDX-FileCopyrightText: 2026 Blender Authors
 * SPDX-License-Identifier: Apache-2.0 */
#pragma once

/* Exact public-canonical SDK fixtures; SHA256 checks bind every byte in the tests. */
namespace materialx_standard_surface_scalar_fixtures {
struct Fixture {
  const char *name;
  const char *sha256;
  const char *usda;
};
static const Fixture fixtures[] = {
  {"declared_optional_weights_omitted", "15c94881fd99d3171d65771e9329878da3a49c7d0d80d5258fa6306db71f7102",
    R"MXSCALAR(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:surface.connect = </Looks/Material/Root/Nested/standard_surface.outputs:out>

                def Shader "standard_surface" (
                    customData = {
                        string materialXNodeDef = "ND_standard_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_standard_surface_surfaceshader"
                    float inputs:base = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base"
                        }
                    )
                    color3f inputs:base_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    float inputs:coat (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat"
                        }
                    )
                    float inputs:coat_affect_color = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Color"
                        }
                    )
                    float inputs:coat_affect_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Roughness"
                        }
                    )
                    float inputs:coat_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
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
                    float inputs:coat_IOR = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    vector3f inputs:coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Coat"
                            string uiname = "Coat normal"
                        }
                    )
                    float inputs:coat_rotation = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Rotation"
                        }
                    )
                    float inputs:coat_roughness = 0.1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Diffuse Roughness"
                        }
                    )
                    float inputs:emission = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                   )MXSCALAR"
    R"MXSCALAR( )
                    float inputs:metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Metalness"
                        }
                    )
                    vector3f inputs:normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    color3f inputs:opacity = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Geometry"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Opacity"
                        }
                    )
                    float inputs:sheen (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Sheen"
                        }
                    )
                    color3f inputs:sheen_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Sheen Color"
                        }
                    )
                    float inputs:sheen_roughness = 0.3 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Sheen Roughness"
                        }
                    )
                    float inputs:specular = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular"
                        }
                    )
                    float inputs:specular_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
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
                    float inputs:specular_IOR = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_rotation = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Rotation"
                        }
                    )
                    float inputs:specular_roughness = 0.2 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:subsurface = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface"
                        }
                    )
                    float inputs:subsurface_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    color3f inputs:subsurface_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    color3f inputs:subsurface_radius = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced)MXSCALAR"
    R"MXSCALAR( = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius"
                        }
                    )
                    float inputs:subsurface_scale = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Scale"
                            string uisoftmax = "10.0"
                        }
                    )
                    vector3f inputs:tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent Input"
                        }
                    )
                    float inputs:thin_film_IOR = 1.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "2000.0"
                        }
                    )
                    bool inputs:thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    float inputs:transmission (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission"
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
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_dispersion = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion"
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_extra_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Roughness"
                            string uisoftmin = "0.0"
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
                            string uimin = "0.0"
                            string uiname = "Transmission Anisotropy"
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

)MXSCALAR"},
  {"declared_roughness_value_omitted", "796aae481b2311fe5553e4a445d76434b66fa742dd4090ea58e8970e8a5c6a8c",
    R"MXSCALAR(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:surface.connect = </Looks/Material/Root/Nested/standard_surface.outputs:out>

                def Shader "standard_surface" (
                    customData = {
                        string materialXNodeDef = "ND_standard_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_standard_surface_surfaceshader"
                    float inputs:base = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base"
                        }
                    )
                    color3f inputs:base_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    float inputs:coat = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat"
                        }
                    )
                    float inputs:coat_affect_color = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Color"
                        }
                    )
                    float inputs:coat_affect_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Roughness"
                        }
                    )
                    float inputs:coat_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
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
                    float inputs:coat_IOR = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    vector3f inputs:coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Coat"
                            string uiname = "Coat normal"
                        }
                    )
                    float inputs:coat_rotation = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Rotation"
                        }
                    )
                    float inputs:coat_roughness = 0.1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Diffuse Roughness"
                        }
                    )
                    float inputs:emission = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
               )MXSCALAR"
    R"MXSCALAR(     )
                    float inputs:metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Metalness"
                        }
                    )
                    vector3f inputs:normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    color3f inputs:opacity = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Geometry"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Opacity"
                        }
                    )
                    float inputs:sheen = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Sheen"
                        }
                    )
                    color3f inputs:sheen_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Sheen Color"
                        }
                    )
                    float inputs:sheen_roughness = 0.3 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Sheen Roughness"
                        }
                    )
                    float inputs:specular = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular"
                        }
                    )
                    float inputs:specular_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
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
                    float inputs:specular_IOR = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_rotation = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Rotation"
                        }
                    )
                    float inputs:specular_roughness (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:subsurface = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface"
                        }
                    )
                    float inputs:subsurface_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    color3f inputs:subsurface_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    color3f inputs:subsurface_radius = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanc)MXSCALAR"
    R"MXSCALAR(ed = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius"
                        }
                    )
                    float inputs:subsurface_scale = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Scale"
                            string uisoftmax = "10.0"
                        }
                    )
                    vector3f inputs:tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent Input"
                        }
                    )
                    float inputs:thin_film_IOR = 1.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "2000.0"
                        }
                    )
                    bool inputs:thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    float inputs:transmission = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission"
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
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_dispersion = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion"
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_extra_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Roughness"
                            string uisoftmin = "0.0"
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
                            string uimin = "0.0"
                            string uiname = "Transmission Anisotropy"
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

)MXSCALAR"},
  {"fully_authored_defaults", "5eb2c0ba62507c820d2c0637f2548ed9597adc4d4bedf0589031a05bf7124ca3",
    R"MXSCALAR(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                token outputs:surface.connect = </Looks/Material/Root/Nested/standard_surface.outputs:out>

                def Shader "standard_surface" (
                    customData = {
                        string materialXNodeDef = "ND_standard_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_standard_surface_surfaceshader"
                    float inputs:base = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base"
                        }
                    )
                    color3f inputs:base_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    float inputs:coat = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat"
                        }
                    )
                    float inputs:coat_affect_color = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Color"
                        }
                    )
                    float inputs:coat_affect_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Roughness"
                        }
                    )
                    float inputs:coat_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
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
                    float inputs:coat_IOR = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    vector3f inputs:coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Coat"
                            string uiname = "Coat normal"
                        }
                    )
                    float inputs:coat_rotation = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Rotation"
                        }
                    )
                    float inputs:coat_roughness = 0.1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Diffuse Roughness"
                        }
                    )
                    float inputs:emission = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
               )MXSCALAR"
    R"MXSCALAR(     )
                    float inputs:metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Metalness"
                        }
                    )
                    vector3f inputs:normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    color3f inputs:opacity = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Geometry"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Opacity"
                        }
                    )
                    float inputs:sheen = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Sheen"
                        }
                    )
                    color3f inputs:sheen_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Sheen Color"
                        }
                    )
                    float inputs:sheen_roughness = 0.3 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Sheen Roughness"
                        }
                    )
                    float inputs:specular = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular"
                        }
                    )
                    float inputs:specular_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
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
                    float inputs:specular_IOR = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_rotation = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Rotation"
                        }
                    )
                    float inputs:specular_roughness = 0.2 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:subsurface = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface"
                        }
                    )
                    float inputs:subsurface_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    color3f inputs:subsurface_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    color3f inputs:subsurface_radius = (1, 1, 1) (
                        sdrMetadata = {
                            string ui)MXSCALAR"
    R"MXSCALAR(advanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius"
                        }
                    )
                    float inputs:subsurface_scale = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Scale"
                            string uisoftmax = "10.0"
                        }
                    )
                    vector3f inputs:tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent Input"
                        }
                    )
                    float inputs:thin_film_IOR = 1.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "2000.0"
                        }
                    )
                    bool inputs:thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    float inputs:transmission = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission"
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
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_dispersion = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion"
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_extra_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Roughness"
                            string uisoftmin = "0.0"
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
                            string uimin = "0.0"
                            string uiname = "Transmission Anisotropy"
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

)MXSCALAR"},
};
}  // namespace materialx_standard_surface_scalar_fixtures
