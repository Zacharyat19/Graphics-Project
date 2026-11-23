#version 120

varying vec3 vFragPos;
varying vec3 vNormal;

void main()
{
    vec3 N = normalize(vNormal);

    vec3 V = normalize(-vFragPos);

    vec4 color = gl_FrontLightModelProduct.sceneColor;

    for (int i = 0; i < gl_MaxLights; ++i)
    {
        vec3 L;
        float attenuation = 1.0;

        if (gl_LightSource[i].position.w == 0.0)
        {
            // Directional light
            L = normalize(gl_LightSource[i].position.xyz);
            continue;
        }
        else
        {
            // Positional light
            L = gl_LightSource[i].position.xyz - vFragPos;
            float dist = length(L);
            L = L / dist;

            attenuation = 1.0 / (gl_LightSource[i].constantAttenuation +
                                 gl_LightSource[i].linearAttenuation    * dist +
                                 gl_LightSource[i].quadraticAttenuation * dist * dist);

            if (gl_LightSource[i].spotCutoff <= 90.0)
            {
                // Spotlight, ignore
                continue;
            }
        }

        float NdotL = max(dot(N, L), 0.0);

        vec4 ambient = gl_FrontLightProduct[i].ambient;
        vec4 diffuse = gl_FrontLightProduct[i].diffuse * NdotL;
        vec4 specular = vec4(0.0);

        if (NdotL > 0.0)
        {
            vec3 H = normalize(L + V);
            float NdotH = max(dot(N, H), 0.0);
            float shininess = max(gl_FrontMaterial.shininess, 1e-4);
            specular = gl_FrontLightProduct[i].specular * pow(NdotH, shininess);
        }

        color += attenuation * (ambient + diffuse + specular);
    }

    gl_FragColor = vec4(color.rgb, 1.0);
}
