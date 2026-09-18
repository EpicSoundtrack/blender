/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

// Exact official MaterialX 1.39.4 default/USDA fixtures; source hashes retained.
#pragma once
namespace ss_completion_data {
struct Default { const char *name; const char *kind; float scalar100, scalar101; float3 color100, color101; bool restricted; };
static const Default defaults[] = {
  {"base","float",0.8f,1.0f,zero_float3(),zero_float3(),false},
  {"base_color","color3",0.0f,0.0f,make_float3(1.0f,1.0f,1.0f),make_float3(0.8f,0.8f,0.8f),false},
  {"diffuse_roughness","float",0.0f,0.0f,zero_float3(),zero_float3(),false},
  {"metalness","float",0.0f,0.0f,zero_float3(),zero_float3(),false},
  {"specular","float",1.0f,1.0f,zero_float3(),zero_float3(),false},
  {"specular_color","color3",0.0f,0.0f,make_float3(1.0f,1.0f,1.0f),make_float3(1.0f,1.0f,1.0f),false},
  {"specular_roughness","float",0.2f,0.2f,zero_float3(),zero_float3(),false},
  {"specular_IOR","float",1.5f,1.5f,zero_float3(),zero_float3(),false},
  {"specular_anisotropy","float",0.0f,0.0f,zero_float3(),zero_float3(),false},
  {"specular_rotation","float",0.0f,0.0f,zero_float3(),zero_float3(),false},
  {"transmission","float",0.0f,0.0f,zero_float3(),zero_float3(),false},
  {"transmission_color","color3",0.0f,0.0f,make_float3(1.0f,1.0f,1.0f),make_float3(1.0f,1.0f,1.0f),false},
  {"transmission_depth","float",0.0f,0.0f,zero_float3(),zero_float3(),true},
  {"transmission_scatter","color3",0.0f,0.0f,make_float3(0.0f,0.0f,0.0f),make_float3(0.0f,0.0f,0.0f),true},
  {"transmission_scatter_anisotropy","float",0.0f,0.0f,zero_float3(),zero_float3(),true},
  {"transmission_dispersion","float",0.0f,0.0f,zero_float3(),zero_float3(),true},
  {"transmission_extra_roughness","float",0.0f,0.0f,zero_float3(),zero_float3(),true},
  {"subsurface","float",0.0f,0.0f,zero_float3(),zero_float3(),false},
  {"subsurface_color","color3",0.0f,0.0f,make_float3(1.0f,1.0f,1.0f),make_float3(1.0f,1.0f,1.0f),true},
  {"subsurface_radius","color3",0.0f,0.0f,make_float3(1.0f,1.0f,1.0f),make_float3(1.0f,1.0f,1.0f),false},
  {"subsurface_scale","float",1.0f,1.0f,zero_float3(),zero_float3(),false},
  {"subsurface_anisotropy","float",0.0f,0.0f,zero_float3(),zero_float3(),false},
  {"sheen","float",0.0f,0.0f,zero_float3(),zero_float3(),false},
  {"sheen_color","color3",0.0f,0.0f,make_float3(1.0f,1.0f,1.0f),make_float3(1.0f,1.0f,1.0f),false},
  {"sheen_roughness","float",0.3f,0.3f,zero_float3(),zero_float3(),false},
  {"coat","float",0.0f,0.0f,zero_float3(),zero_float3(),false},
  {"coat_color","color3",0.0f,0.0f,make_float3(1.0f,1.0f,1.0f),make_float3(1.0f,1.0f,1.0f),false},
  {"coat_roughness","float",0.1f,0.1f,zero_float3(),zero_float3(),false},
  {"coat_anisotropy","float",0.0f,0.0f,zero_float3(),zero_float3(),true},
  {"coat_rotation","float",0.0f,0.0f,zero_float3(),zero_float3(),true},
  {"coat_IOR","float",1.5f,1.5f,zero_float3(),zero_float3(),false},
  {"coat_affect_color","float",0.0f,0.0f,zero_float3(),zero_float3(),true},
  {"coat_affect_roughness","float",0.0f,0.0f,zero_float3(),zero_float3(),true},
  {"thin_film_thickness","float",0.0f,0.0f,zero_float3(),zero_float3(),false},
  {"thin_film_IOR","float",1.5f,1.5f,zero_float3(),zero_float3(),false},
  {"emission","float",0.0f,0.0f,zero_float3(),zero_float3(),false},
  {"emission_color","color3",0.0f,0.0f,make_float3(1.0f,1.0f,1.0f),make_float3(1.0f,1.0f,1.0f),false},
  {"opacity","color3",0.0f,0.0f,make_float3(1.0f,1.0f,1.0f),make_float3(1.0f,1.0f,1.0f),false},
  {"thin_walled","boolean",0.0f,0.0f,zero_float3(),zero_float3(),false},
};
struct Fixture { const char *name; const char *sha; const char *usda; bool legacy; };
static const Fixture fixtures[] = {
  {"v100_full","8493be438f0379d3cf95b5cd35ef50fa3f86d7be51fe04ba59ba5a805e6a5a9a",
    R"MXDFLT(#usda 1.0

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
                        string materialXNodeDef = "ND_standard_surface_surfaceshader_100"
                    }
                )
                {
                    uniform token info:id = "ND_standard_surface_surfaceshader_100"
                    float inputs:base = 0.8 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base"
                        }
                    )
                    color3f inputs:base_color = (1, 1, 1) (
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
           )MXDFLT"
    R"MXDFLT(         )
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
                            strin)MXDFLT"
    R"MXDFLT(g uiadvanced = "true"
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

)MXDFLT"
    ,true},
  {"v100_optional_unvalued","a7ab091c583245a14d80889e8873eb5f275bf16b9408a21dd401fcc6723a1ffd",
    R"MXDFLT(#usda 1.0

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
                        string materialXNodeDef = "ND_standard_surface_surfaceshader_100"
                    }
                )
                {
                    uniform token info:id = "ND_standard_surface_surfaceshader_100"
                    float inputs:base = 0.8 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base"
                        }
                    )
                    color3f inputs:base_color = (1, 1, 1) (
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
                    color3f inputs:coat_color (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_IOR (
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
                    float inputs:coat_roughness (
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
                    )
                 )MXDFLT"
    R"MXDFLT(   float inputs:metalness = 0 (
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
                    color3f inputs:sheen_color (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Sheen Color"
                        }
                    )
                    float inputs:sheen_roughness (
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
                            string uiadvanced = "true"
                            )MXDFLT"
    R"MXDFLT(string uifolder = "Subsurface"
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
                    color3f inputs:transmission_color (
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

)MXDFLT"
    ,true},
  {"v100_all_defaults_unvalued","5e027784c376d1a7f35eed5887a27a140fd3f98cc0498734baa9e2d453aecb94",
    R"MXDFLT(#usda 1.0

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
                        string materialXNodeDef = "ND_standard_surface_surfaceshader_100"
                    }
                )
                {
                    uniform token info:id = "ND_standard_surface_surfaceshader_100"
                    float inputs:base (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base"
                        }
                    )
                    color3f inputs:base_color (
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
                    float inputs:coat_affect_color (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Color"
                        }
                    )
                    float inputs:coat_affect_roughness (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Roughness"
                        }
                    )
                    float inputs:coat_anisotropy (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    color3f inputs:coat_color (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_IOR (
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
                    float inputs:coat_rotation (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Rotation"
                        }
                    )
                    float inputs:coat_roughness (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:diffuse_roughness (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Diffuse Roughness"
                        }
                    )
                    float inputs:emission (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:emission_color (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:metalness (
                        sd)MXDFLT"
    R"MXDFLT(rMetadata = {
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
                    color3f inputs:opacity (
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
                    color3f inputs:sheen_color (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Sheen Color"
                        }
                    )
                    float inputs:sheen_roughness (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Sheen Roughness"
                        }
                    )
                    float inputs:specular (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular"
                        }
                    )
                    float inputs:specular_anisotropy (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    color3f inputs:specular_color (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_IOR (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_rotation (
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
                    float inputs:subsurface (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface"
                        }
                    )
                    float inputs:subsurface_anisotropy (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    color3f inputs:subsurface_color (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    color3f inputs:subsurface_radius (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
     )MXDFLT"
    R"MXDFLT(                       string uiname = "Subsurface Radius"
                        }
                    )
                    float inputs:subsurface_scale (
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
                    float inputs:thin_film_IOR (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "2000.0"
                        }
                    )
                    bool inputs:thin_walled (
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
                    color3f inputs:transmission_color (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_dispersion (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion"
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_extra_roughness (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Roughness"
                            string uisoftmin = "0.0"
                        }
                    )
                    color3f inputs:transmission_scatter (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy (
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

)MXDFLT"
    ,true},
  {"v101_full","5eb2c0ba62507c820d2c0637f2548ed9597adc4d4bedf0589031a05bf7124ca3",
    R"MXDFLT(#usda 1.0

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
               )MXDFLT"
    R"MXDFLT(     )
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
                            string ui)MXDFLT"
    R"MXDFLT(advanced = "true"
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

)MXDFLT"
    ,false},
  {"v101_optional_unvalued","c7a3d5ad4e4a58044e9dc25c982f3fcd02af22b994e7127b9f64031a86e5013e",
    R"MXDFLT(#usda 1.0

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
                    color3f inputs:coat_color (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_IOR (
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
                    float inputs:coat_roughness (
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
                    )
                    f)MXDFLT"
    R"MXDFLT(loat inputs:metalness = 0 (
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
                    color3f inputs:sheen_color (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Sheen Color"
                        }
                    )
                    float inputs:sheen_roughness (
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
                            string uiadvanced = "true"
                            stri)MXDFLT"
    R"MXDFLT(ng uifolder = "Subsurface"
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
                    color3f inputs:transmission_color (
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

)MXDFLT"
    ,false},
  {"v101_all_defaults_unvalued","2e245e050bc8d7511d956d68aaba28675ab861f6aedae85854f539ed6064eb4a",
    R"MXDFLT(#usda 1.0

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
                    float inputs:base (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base"
                        }
                    )
                    color3f inputs:base_color (
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
                    float inputs:coat_affect_color (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Color"
                        }
                    )
                    float inputs:coat_affect_roughness (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Roughness"
                        }
                    )
                    float inputs:coat_anisotropy (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    color3f inputs:coat_color (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_IOR (
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
                    float inputs:coat_rotation (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Rotation"
                        }
                    )
                    float inputs:coat_roughness (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:diffuse_roughness (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Diffuse Roughness"
                        }
                    )
                    float inputs:emission (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:emission_color (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:metalness (
                        sdrMetadat)MXDFLT"
    R"MXDFLT(a = {
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
                    color3f inputs:opacity (
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
                    color3f inputs:sheen_color (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Sheen Color"
                        }
                    )
                    float inputs:sheen_roughness (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Sheen Roughness"
                        }
                    )
                    float inputs:specular (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular"
                        }
                    )
                    float inputs:specular_anisotropy (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    color3f inputs:specular_color (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_IOR (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_rotation (
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
                    float inputs:subsurface (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface"
                        }
                    )
                    float inputs:subsurface_anisotropy (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    color3f inputs:subsurface_color (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    color3f inputs:subsurface_radius (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
             )MXDFLT"
    R"MXDFLT(               string uiname = "Subsurface Radius"
                        }
                    )
                    float inputs:subsurface_scale (
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
                    float inputs:thin_film_IOR (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "2000.0"
                        }
                    )
                    bool inputs:thin_walled (
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
                    color3f inputs:transmission_color (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_dispersion (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion"
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_extra_roughness (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Roughness"
                            string uisoftmin = "0.0"
                        }
                    )
                    color3f inputs:transmission_scatter (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy (
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

)MXDFLT"
    ,false},
};
} // namespace ss_completion_data
