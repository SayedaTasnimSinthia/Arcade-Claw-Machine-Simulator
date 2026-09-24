#version 330 core
out vec4 FragColor;

// holds color and shine settings for an object
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;

    bool isChecker;
    vec3 checkerColorA;
    vec3 checkerColorB;
    float tileSize;
};

// settings for a light bulb that shines in all directions
struct PointLight {
    vec3 position;
    float k_c;  // attenuation factors
    float k_l;  // attenuation factors
    float k_q;  // attenuation factors

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// settings for a flashlight beam that points one way
struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    float k_c;
    float k_l;
    float k_q;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

// total number of light bulbs
#define NR_POINT_LIGHTS 2

// input position and surface normal direction from vertex shader
in vec3 FragPos;
in vec3 Normal;

// uniforms passed from c++ program
uniform vec3 viewPos;
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform SpotLight spotLight;
uniform Material material;

// function prototypes
vec3 CalcPointLight(PointLight light, vec3 N, vec3 fragPos, vec3 V, vec3 amb, vec3 diff, vec3 spec, float shininess);
vec3 CalcSpotLight(SpotLight light, vec3 N, vec3 fragPos, vec3 V, vec3 amb, vec3 diff, vec3 spec, float shininess);

void main()
{
    // find surface facing direction and direction looking at camera
    vec3 N = normalize(Normal);
    vec3 V = normalize(viewPos - FragPos);

    // copy base surface colors
    vec3 amb = material.ambient;
    vec3 diff = material.diffuse;

    // make two-color checker tiles on floor using world coordinates
    if (material.isChecker)
    {
        float cx = floor(FragPos.x / material.tileSize);
        float cz = floor(FragPos.z / material.tileSize);
        float checker = mod(cx + cz, 2.0);
        vec3 tile = mix(material.checkerColorA, material.checkerColorB, checker);
        diff = tile;
        amb = tile * 0.35;
    }

    // add light from each point light bulb
    vec3 result = vec3(0.0);
    for (int i = 0; i < NR_POINT_LIGHTS; i++)
        result += CalcPointLight(pointLights[i], N, FragPos, V, amb, diff, material.specular, material.shininess);

    // add light from the flashlight beam and write final pixel color
    result += CalcSpotLight(spotLight, N, FragPos, V, amb, diff, material.specular, material.shininess);
    FragColor = vec4(result, 1.0);
}

// calculates color and brightness from one point light bulb
vec3 CalcPointLight(PointLight light, vec3 N, vec3 fragPos, vec3 V, vec3 amb, vec3 diff, vec3 spec, float shininess)
{
    // find direction pointing to light and direction light bounces off
    vec3 L = normalize(light.position - fragPos);
    vec3 R = reflect(-L, N);

    // measure distance to light and make light fade over distance
    float d = length(light.position - fragPos);
    float attenuation = 1.0 / (light.k_c + light.k_l * d + light.k_q * (d * d));

    // calculate shadow base color direct brightness and bright shine spot
    vec3 ambient  = amb * light.ambient;
    vec3 diffuse  = diff * max(dot(N, L), 0.0) * light.diffuse;
    vec3 specular = spec * pow(max(dot(V, R), 0.0), shininess) * light.specular;

    // combine all colors and make them darker when far away
    return (ambient + diffuse + specular) * attenuation;
}

// calculates color and brightness from a flashlight beam
vec3 CalcSpotLight(SpotLight light, vec3 N, vec3 fragPos, vec3 V, vec3 amb, vec3 diff, vec3 spec, float shininess)
{
    // find direction pointing to light and direction light bounces off
    vec3 L = normalize(light.position - fragPos);
    vec3 R = reflect(-L, N);

    // check if point is inside the round spotlight circle and soften edges
    float theta = dot(L, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    // measure distance to light and make light fade over distance
    float d = length(light.position - fragPos);
    float attenuation = 1.0 / (light.k_c + light.k_l * d + light.k_q * (d * d));

    // calculate shadow base color direct brightness and bright shine spot
    vec3 ambient  = amb * light.ambient;
    vec3 diffuse  = diff * max(dot(N, L), 0.0) * light.diffuse;
    vec3 specular = spec * pow(max(dot(V, R), 0.0), shininess) * light.specular;

    // combine colors soften beam edges and make darker when far away
    return (ambient + (diffuse + specular) * intensity) * attenuation;
}