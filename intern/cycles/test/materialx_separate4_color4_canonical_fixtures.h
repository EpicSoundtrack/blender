/* SPDX-License-Identifier: Apache-2.0 */
/* Exact retained public-authoring canonical USDA from the 2026-10-23 Separate4
 * Color4 crash cohort. CPU reader/lowerer regression only; no parity credit. */
#pragma once

namespace materialx_separate4_color4_fixtures {
struct Fixture { const char *case_id; const char *sha256; const char *usda; };
inline constexpr Fixture cases[] = {
  {"L0", "93b7d530dfd9bb43381ed69f969667bc9cedde2b960093cee39eecc313b1b4a3", R"MXLITERAL(#usda 1.0

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
                token outputs:NG_SeparateAllOutputs__material_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:NG_SeparateAllOutputs__material_surfaceshader.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>

                def Shader "NG_SeparateAllOutputs__under_test" (
                    customData = {
                        string materialXNodeDef = "ND_separate4_color4"
                    }
                )
                {
                    uniform token info:id = "ND_separate4_color4"
                    color4f inputs:in = (10.125, 11.125, 12.125, 13.125)
                    float outputs:outa
                    float outputs:outb
                    float outputs:outg
                    float outputs:outr
                }

                def Shader "NG_SeparateAllOutputs__lower0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float inputs:value2 = 10.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 10.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass0" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper0.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float inputs:value2 = 11.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 11.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass1" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower1.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper1.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float inputs:value2 = 12.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 12.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass2" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper2.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float inputs:value2 = 13.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 13.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass3" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower3.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass23" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__rgb" (
                    customData = {
                        string materialXNodeDef = "ND_combine3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_combine3_color3"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass1.outputs:out>
                    float inputs:in3.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass23.outputs:out>
                    color3f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__surface" (
                    customData = {
                        string materialXNodeDef = "ND_convert_color3_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_convert_color3_surfaceshader"
                    color3f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__rgb.outputs:out>
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

)MXLITERAL"},
  {"K0", "afffa8bee4b3881913a90dc8f63146eaeaaa2027fc565687e93c9b8340b6eed2", R"MXLITERAL(#usda 1.0

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
                token outputs:NG_SeparateAllOutputs__material_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:NG_SeparateAllOutputs__material_surfaceshader.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>

                def Shader "NG_SeparateAllOutputs__operand" (
                    customData = {
                        string materialXNodeDef = "ND_constant_color4"
                    }
                )
                {
                    uniform token info:id = "ND_constant_color4"
                    color4f inputs:value = (10.125, 11.125, 12.125, 13.125)
                    color4f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__under_test" (
                    customData = {
                        string materialXNodeDef = "ND_separate4_color4"
                    }
                )
                {
                    uniform token info:id = "ND_separate4_color4"
                    color4f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__operand.outputs:out>
                    float outputs:outa
                    float outputs:outb
                    float outputs:outg
                    float outputs:outr
                }

                def Shader "NG_SeparateAllOutputs__lower0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float inputs:value2 = 10.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 10.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass0" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper0.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float inputs:value2 = 11.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 11.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass1" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower1.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper1.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float inputs:value2 = 12.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 12.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass2" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper2.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float inputs:value2 = 13.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 13.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass3" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower3.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass23" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__rgb" (
                    customData = {
                        string materialXNodeDef = "ND_combine3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_combine3_color3"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass1.outputs:out>
                    float inputs:in3.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass23.outputs:out>
                    color3f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__surface" (
                    customData = {
                        string materialXNodeDef = "ND_convert_color3_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_convert_color3_surfaceshader"
                    color3f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__rgb.outputs:out>
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

)MXLITERAL"},
  {"L1", "160b4264d649af84070f3d9831e9e6de18aab4bb10d1eb80f1bfa71856455d94", R"MXLITERAL(#usda 1.0

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
                token outputs:NG_SeparateAllOutputs__material_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:NG_SeparateAllOutputs__material_surfaceshader.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>

                def Shader "NG_SeparateAllOutputs__under_test" (
                    customData = {
                        string materialXNodeDef = "ND_separate4_color4"
                    }
                )
                {
                    uniform token info:id = "ND_separate4_color4"
                    color4f inputs:in = (20.125, 21.125, 22.125, 23.125)
                    float outputs:outa
                    float outputs:outb
                    float outputs:outg
                    float outputs:outr
                }

                def Shader "NG_SeparateAllOutputs__lower0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float inputs:value2 = 20.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 20.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass0" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper0.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float inputs:value2 = 21.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 21.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass1" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower1.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper1.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float inputs:value2 = 22.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 22.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass2" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper2.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float inputs:value2 = 23.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 23.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass3" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower3.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass23" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__rgb" (
                    customData = {
                        string materialXNodeDef = "ND_combine3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_combine3_color3"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass1.outputs:out>
                    float inputs:in3.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass23.outputs:out>
                    color3f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__surface" (
                    customData = {
                        string materialXNodeDef = "ND_convert_color3_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_convert_color3_surfaceshader"
                    color3f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__rgb.outputs:out>
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

)MXLITERAL"},
  {"K1", "628b9eb12a7725615b8f4b9b05c9c5c2884e8e55f15642f2fc9ea9106a15fddd", R"MXLITERAL(#usda 1.0

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
                token outputs:NG_SeparateAllOutputs__material_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:NG_SeparateAllOutputs__material_surfaceshader.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>

                def Shader "NG_SeparateAllOutputs__operand" (
                    customData = {
                        string materialXNodeDef = "ND_constant_color4"
                    }
                )
                {
                    uniform token info:id = "ND_constant_color4"
                    color4f inputs:value = (20.125, 21.125, 22.125, 23.125)
                    color4f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__under_test" (
                    customData = {
                        string materialXNodeDef = "ND_separate4_color4"
                    }
                )
                {
                    uniform token info:id = "ND_separate4_color4"
                    color4f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__operand.outputs:out>
                    float outputs:outa
                    float outputs:outb
                    float outputs:outg
                    float outputs:outr
                }

                def Shader "NG_SeparateAllOutputs__lower0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float inputs:value2 = 20.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 20.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass0" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper0.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float inputs:value2 = 21.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 21.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass1" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower1.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper1.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float inputs:value2 = 22.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 22.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass2" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper2.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float inputs:value2 = 23.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 23.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass3" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower3.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass23" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__rgb" (
                    customData = {
                        string materialXNodeDef = "ND_combine3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_combine3_color3"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass1.outputs:out>
                    float inputs:in3.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass23.outputs:out>
                    color3f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__surface" (
                    customData = {
                        string materialXNodeDef = "ND_convert_color3_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_convert_color3_surfaceshader"
                    color3f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__rgb.outputs:out>
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

)MXLITERAL"},
  {"L2", "59393611933753d1480343bbf0e21b3237279dfb45e2670a1edc4dc063381316", R"MXLITERAL(#usda 1.0

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
                token outputs:NG_SeparateAllOutputs__material_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:NG_SeparateAllOutputs__material_surfaceshader.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>

                def Shader "NG_SeparateAllOutputs__under_test" (
                    customData = {
                        string materialXNodeDef = "ND_separate4_color4"
                    }
                )
                {
                    uniform token info:id = "ND_separate4_color4"
                    color4f inputs:in = (30.125, 31.125, 32.125, 33.125)
                    float outputs:outa
                    float outputs:outb
                    float outputs:outg
                    float outputs:outr
                }

                def Shader "NG_SeparateAllOutputs__lower0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float inputs:value2 = 30.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 30.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass0" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper0.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float inputs:value2 = 31.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 31.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass1" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower1.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper1.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float inputs:value2 = 32.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 32.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass2" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper2.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float inputs:value2 = 33.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 33.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass3" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower3.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass23" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__rgb" (
                    customData = {
                        string materialXNodeDef = "ND_combine3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_combine3_color3"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass1.outputs:out>
                    float inputs:in3.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass23.outputs:out>
                    color3f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__surface" (
                    customData = {
                        string materialXNodeDef = "ND_convert_color3_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_convert_color3_surfaceshader"
                    color3f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__rgb.outputs:out>
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

)MXLITERAL"},
  {"K2", "e628481bba0407fdb37df80bb677e67defe7802eec944c35b2843b92c7bb5ba5", R"MXLITERAL(#usda 1.0

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
                token outputs:NG_SeparateAllOutputs__material_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:NG_SeparateAllOutputs__material_surfaceshader.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>

                def Shader "NG_SeparateAllOutputs__operand" (
                    customData = {
                        string materialXNodeDef = "ND_constant_color4"
                    }
                )
                {
                    uniform token info:id = "ND_constant_color4"
                    color4f inputs:value = (30.125, 31.125, 32.125, 33.125)
                    color4f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__under_test" (
                    customData = {
                        string materialXNodeDef = "ND_separate4_color4"
                    }
                )
                {
                    uniform token info:id = "ND_separate4_color4"
                    color4f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__operand.outputs:out>
                    float outputs:outa
                    float outputs:outb
                    float outputs:outg
                    float outputs:outr
                }

                def Shader "NG_SeparateAllOutputs__lower0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float inputs:value2 = 30.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 30.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass0" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper0.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float inputs:value2 = 31.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 31.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass1" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower1.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper1.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float inputs:value2 = 32.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 32.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass2" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper2.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float inputs:value2 = 33.1249
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 33.1251
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass3" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower3.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass23" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__rgb" (
                    customData = {
                        string materialXNodeDef = "ND_combine3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_combine3_color3"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass1.outputs:out>
                    float inputs:in3.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass23.outputs:out>
                    color3f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__surface" (
                    customData = {
                        string materialXNodeDef = "ND_convert_color3_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_convert_color3_surfaceshader"
                    color3f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__rgb.outputs:out>
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

)MXLITERAL"},
  {"D0", "8fab3a2da2d7054a333a07f747b1e07e05edaac22ec5e93590bec051a3a04284", R"MXLITERAL(#usda 1.0

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
                token outputs:NG_SeparateAllOutputs__material_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:NG_SeparateAllOutputs__material_surfaceshader.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>

                def Shader "NG_SeparateAllOutputs__under_test" (
                    customData = {
                        string materialXNodeDef = "ND_separate4_color4"
                    }
                )
                {
                    uniform token info:id = "ND_separate4_color4"
                    color4f inputs:in = (0, 0, 0, 0)
                    float outputs:outa
                    float outputs:outb
                    float outputs:outg
                    float outputs:outr
                }

                def Shader "NG_SeparateAllOutputs__lower0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float inputs:value2 = -0.0001
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 0.0001
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass0" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper0.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float inputs:value2 = -0.0001
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 0.0001
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass1" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower1.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper1.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float inputs:value2 = -0.0001
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 0.0001
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass2" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper2.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float inputs:value2 = -0.0001
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1 = 0.0001
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass3" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower3.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass23" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__rgb" (
                    customData = {
                        string materialXNodeDef = "ND_combine3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_combine3_color3"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass1.outputs:out>
                    float inputs:in3.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass23.outputs:out>
                    color3f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__surface" (
                    customData = {
                        string materialXNodeDef = "ND_convert_color3_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_convert_color3_surfaceshader"
                    color3f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__rgb.outputs:out>
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

)MXLITERAL"},
  {"V0", "144c5b0efb8efcd4eb876bacce7be80e7aec9f4bb0591f71be912593a72d03c9", R"MXLITERAL(#usda 1.0

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
                token outputs:NG_SeparateAllOutputs__material_surfaceshader (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:NG_SeparateAllOutputs__material_surfaceshader.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>
                token outputs:surface (
                    customData = {
                        string materialXType = "surfaceshader"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__surface.outputs:out>

                def Shader "NG_SeparateAllOutputs__operand0" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_input_c0" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__operand1" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_input_c1" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__operand2" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_input_c2" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__operand3" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_input_c3" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__operand" (
                    customData = {
                        string materialXNodeDef = "ND_combine4_color4"
                    }
                )
                {
                    uniform token info:id = "ND_combine4_color4"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__operand0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__operand1.outputs:out>
                    float inputs:in3.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__operand2.outputs:out>
                    float inputs:in4.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__operand3.outputs:out>
                    color4f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__under_test" (
                    customData = {
                        string materialXNodeDef = "ND_separate4_color4"
                    }
                )
                {
                    uniform token info:id = "ND_separate4_color4"
                    color4f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__operand.outputs:out>
                    float outputs:outa
                    float outputs:outb
                    float outputs:outg
                    float outputs:outr
                }

                def Shader "NG_SeparateAllOutputs__lower0_reader" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_bound_lower0" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper0_reader" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_bound_upper0" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower0_reader.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper0" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper0_reader.outputs:out>
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outr>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass0" (
                    customData = {
)MXLITERAL"
    R"MXLITERAL(                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper0.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower1_reader" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_bound_lower1" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper1_reader" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_bound_upper1" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower1_reader.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper1" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper1_reader.outputs:out>
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outg>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass1" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower1.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper1.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower2_reader" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_bound_lower2" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper2_reader" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_bound_upper2" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower2_reader.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper2" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper2_reader.outputs:out>
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outb>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass2" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper2.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower3_reader" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_bound_lower3" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper3_reader" (
                    customData = {
                        string materialXNodeDef = "ND_geompropvalue_float"
                    }
                )
                {
)MXLITERAL"
    R"MXLITERAL(                    uniform token info:id = "ND_geompropvalue_float"
                    float inputs:default = 0
                    uniform string inputs:geomprop = "separate_bound_upper3" (
                        sdrMetadata = {
                            string uniform = "true"
                        }
                    )
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__lower3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower3_reader.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__upper3" (
                    customData = {
                        string materialXNodeDef = "ND_ifgreatereq_float"
                    }
                )
                {
                    uniform token info:id = "ND_ifgreatereq_float"
                    float inputs:in1 = 1
                    float inputs:in2 = 0
                    float inputs:value1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper3_reader.outputs:out>
                    float inputs:value2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__under_test.outputs:outa>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass3" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__lower3.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__upper3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__pass23" (
                    customData = {
                        string materialXNodeDef = "ND_multiply_float"
                    }
                )
                {
                    uniform token info:id = "ND_multiply_float"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass2.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass3.outputs:out>
                    float outputs:out
                }

                def Shader "NG_SeparateAllOutputs__rgb" (
                    customData = {
                        string materialXNodeDef = "ND_combine3_color3"
                    }
                )
                {
                    uniform token info:id = "ND_combine3_color3"
                    float inputs:in1.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass0.outputs:out>
                    float inputs:in2.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass1.outputs:out>
                    float inputs:in3.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__pass23.outputs:out>
                    color3f outputs:out
                }

                def Shader "NG_SeparateAllOutputs__surface" (
                    customData = {
                        string materialXNodeDef = "ND_convert_color3_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_convert_color3_surfaceshader"
                    color3f inputs:in.connect = </Looks/Material/Root/Nested/NG_SeparateAllOutputs__rgb.outputs:out>
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

)MXLITERAL"},
};
}  // namespace materialx_separate4_color4_fixtures
