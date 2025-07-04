Shader "Custom/BoardShader" {
    Properties {
        _MainTex ("Texture", 2D) = "white" {}
    }
    SubShader {
        Tags { "RenderType"="Opaque" }
        LOD 100
        Pass {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"
            struct appdata  {
                float4 vertex : POSITION;
                float2 uv : TEXCOORD0;
            };
            struct v2f  {
                float2 uv : TEXCOORD0;
                float4 vertex : SV_POSITION;
            };
            sampler2D _MainTex;
            float4 _MainTex_ST;

            v2f vert (appdata v) {
                v2f o;
                o.vertex = UnityObjectToClipPos(v.vertex);
                o.uv = TRANSFORM_TEX(v.uv, _MainTex);
                return o;
            }

            fixed4 frag (v2f i) : SV_Target {
                float s = i.uv.x;
                float t = i.uv.y;
                float sum = floor(s * 1) + floor(t * 1);
                bool isEven = fmod(sum, 2.0) == 0.0;

                // Chessboard pattern logic
                float percent = (isEven) ? 0.0 : 1.0;
                
                float4 col = float4(1.0, 1.0, 1.0, 1.0);
                return col * percent;
            }
            ENDCG
        }
    }
}
