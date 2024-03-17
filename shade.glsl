#version 460
#extension GL_NV_shader_buffer_load : enable

struct CameraProps {
    vec2 frameSize;
    float fov;
};

struct Ray {
    vec4 start;
    vec4 direction;
};

struct Material {
    vec3 ambient, diffuse, specular;
    float absorption, reflection, transparency;
    float shininess;
};

struct HitRecord {
    Material mat;
    vec4 intersection;
    vec3 normal;
    float time;
};

struct ObjectData {
    Material mat;
    mat4x4 mv, mvInverse, mvInverseTranspose;
    uint type;
};

struct Light {
    vec3 ambient, diffuse, specular;
    vec4 position;
};

uniform CameraProps camera;
uniform uint MAX_BOUNCES;
const uint MAX_OBJECT_COUNT = 16;
uniform uint OBJECT_COUNT;
uniform ObjectData[MAX_OBJECT_COUNT] objs;
const uint MAX_LIGHT_COUNT = 16;
uniform uint LIGHT_COUNT;
uniform Light[MAX_LIGHT_COUNT] lights;

const float MAX_FLOAT = 3.402823466e+38;

bool intersectsWithBoxSide(inout float tMin, inout float tMax, float start, float dir);

// inline void transform(float4* o_vec, const float16* i_mat, const float4* i_vec) {
//     o_vec->x = i_mat->s0 * i_vec->x + i_mat->s4 * i_vec->y + i_mat->s8 * i_vec->z + i_mat->sC * i_vec->w;
//     o_vec->y = i_mat->s1 * i_vec->x + i_mat->s5 * i_vec->y + i_mat->s9 * i_vec->z + i_mat->sD * i_vec->w;
//     o_vec->z = i_mat->s2 * i_vec->x + i_mat->s6 * i_vec->y + i_mat->sA * i_vec->z + i_mat->sE * i_vec->w;
//     o_vec->w = i_mat->s3 * i_vec->x + i_mat->s7 * i_vec->y + i_mat->sB * i_vec->z + i_mat->sF * i_vec->w;
// }

bool raycast(const uint count, const ObjectData[MAX_OBJECT_COUNT] objs, in const Ray viewspaceRay, inout HitRecord hit)
{
    Ray ray;

    for (uint objIndex = 0; objIndex < OBJECT_COUNT && objIndex < MAX_OBJECT_COUNT; ++objIndex) {
        const ObjectData obj = objs[objIndex];

        ray.start = obj.mvInverse * viewspaceRay.start;
        // transform(&ray.start, &obj->mvInverse, &viewspaceRay->start);
        ray.direction = obj.mvInverse * viewspaceRay.direction;
        // transform(&ray.direction, &obj->mvInverse, &viewspaceRay->direction);

        switch (obj.type) {
        case 0: // Sphere
        {
            // Solve quadratic
            float A = ray.direction.x * ray.direction.x +
                ray.direction.y * ray.direction.y +
                ray.direction.z * ray.direction.z;
            float B = 2.0 *
                (ray.direction.x * ray.start.x + ray.direction.y * ray.start.y +
                    ray.direction.z * ray.start.z);
            float C = ray.start.x * ray.start.x + ray.start.y * ray.start.y +
                ray.start.z * ray.start.z - 1.0;

            float radical = B * B - 4.0 * A * C;

            // no intersection
            if (radical < 0) continue;

            float root = sqrt(radical);

            float t1 = (-B - root) / (2.0 * A);
            float t2 = (-B + root) / (2.0 * A);

            float tMin = (t1 >= 0 && t2 >= 0) ? min(t1, t2) : max(t1, t2);
            // object is fully behind camera
            if (tMin < 0) continue;

            if (hit.time < tMin) continue;

            hit.time = tMin;

            vec4 objSpaceIntersection = ray.start + tMin * ray.direction;
            hit.intersection = obj.mv * objSpaceIntersection;
            // transform(&hit->intersection, &obj->mv, &objSpaceIntersection);
            vec4 objSpaceNormal = objSpaceIntersection;
            objSpaceNormal.w = 0.0;
            vec4 normal = obj.mv * objSpaceNormal;
            // transform(&normal, obj->mv, objSpaceNormal);
            hit.normal = normalize(normal.xyz);
            hit.mat = obj.mat;
            continue;
        }

        case 1: // Box
        {
            float txMin, txMax, tyMin, tyMax, tzMin, tzMax;

            if (!intersectsWithBoxSide(txMin, txMax, ray.start.x, ray.direction.x))
                continue;

            if (!intersectsWithBoxSide(tyMin, tyMax, ray.start.y, ray.direction.y))
                continue;

            if (!intersectsWithBoxSide(tzMin, tzMax, ray.start.z, ray.direction.z))
                continue;

            float tMin = max(max(txMin, tyMin), tzMin);
            float tMax = min(min(txMax, tyMax), tzMax);

            // no intersection
            if (tMax < tMin) continue;

            float tHit = (tMin >= 0 && tMax >= 0) ? min(tMin, tMax) : max(tMin, tMax);
            // object is fully behind camera
            if (tHit < 0) continue;

            // already hit a closer object
            if (hit.time <= tHit) continue;

            vec4 objSpaceIntersection = ray.start + tHit * ray.direction;

            vec4 objSpaceNormal = { 0.0, 0.0, 0.0, 0.0 };
            const float BOX_EXTENTS = 0.4998;
            if (objSpaceIntersection.x > BOX_EXTENTS) objSpaceNormal.x += 1.0;
            else if (objSpaceIntersection.x < -BOX_EXTENTS) objSpaceNormal.x -= 1.0;

            if (objSpaceIntersection.y > BOX_EXTENTS) objSpaceNormal.y += 1.0;
            else if (objSpaceIntersection.y < -BOX_EXTENTS) objSpaceNormal.y -= 1.0;

            if (objSpaceIntersection.z > BOX_EXTENTS) objSpaceNormal.z += 1.0;
            else if (objSpaceIntersection.z < -BOX_EXTENTS) objSpaceNormal.z -= 1.0;

            hit.time = tHit;
            hit.intersection = obj.mv * objSpaceIntersection;
            // transform(&hit->intersection, &obj->mv, &objSpaceIntersection);
            vec4 normal = obj.mv * objSpaceNormal;
            // transform(&normal, &obj->mv, &objSpaceNormal);
            hit.normal = normalize(normal.xyz);
            hit.mat = obj.mat;
            continue;
        }

        }
    }

    return (hit.time < MAX_FLOAT);
}

vec3 componentWiseMultiply(const vec3 lhs, const vec3 rhs)
{
    return (vec3)(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z);
}

// assumes normal is normalized
vec3 reflect(const vec3 incident, const vec3 normal) {
    return incident - 2.0 * dot(incident, normal) * normal;
}

void getFragmentRay(out Ray ray)
{
    const float halfWidth = camera.frameSize.x / 2.0f;
    const float halfHeight = camera.frameSize.y / 2.0f;

    ray.start = vec4( 0.0, 0.0, 0.0, 1.0 );
    ray.direction = vec4(gl_FragCoord.x - halfWidth, gl_FragCoord.y - halfHeight, -(halfHeight / tan(camera.fov)), 0.);
}

out vec4 diffuseColor;

float remap(float n)
{
    return (n + 1.) / 2.;
}

void main() {
    Ray ray;
    getFragmentRay(ray);

    HitRecord hit;
    hit.time = MAX_FLOAT;

    if (!raycast(OBJECT_COUNT, objs, ray, hit))
    {
        diffuseColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // hit test
    //diffuseColor = vec4(1.0, 1.0, 1.0, 1.0);
    //return;

    // shade using normals
    //diffuseColor = vec4((hit.normal + vec3( 1.0, 1.0, 1.0 )) * 0.5f, 1.f);
    //return;

    // shade using ambient
    //diffuseColor = vec4(hit.mat.ambient, 1.f);
    //return;

    vec3 fPosition = hit.intersection.xyz;
    vec3 fNormal = hit.normal;
    vec3 fColor = vec3( 0.0, 0.0, 0.0 );
    vec3 lightVec = vec3( 0.0, 0.0, 0.0 ), viewVec = vec3( 0.0, 0.0, 0.0 ), reflectVec = vec3( 0.0, 0.0, 0.0 );
    vec3 normalView = vec3( 0.0, 0.0, 0.0 );
    vec3 ambient = vec3( 0.0, 0.0, 0.0 ), diffuse = vec3( 0.0, 0.0, 0.0 ), specular = vec3( 0.0, 0.0, 0.0 );
    float nDotL, rDotV;

    vec3 absorbColor = vec3( 0.0, 0.0, 0.0 ), reflectColor = vec3( 0.0, 0.0, 0.0 ), transparencyColor = vec3( 0.0, 0.0, 0.0 );

    for (uint lightIndex = 0; lightIndex < LIGHT_COUNT; ++lightIndex) {
        const Light light = lights[lightIndex];
        if (light.position.w != 0)
            lightVec = light.position.xyz - fPosition.xyz;
        else
            lightVec = -light.position.xyz;

        // Shoot ray towards light source, any hit means shadow.
        Ray rayToLight;
        rayToLight.start = vec4(fPosition, 1.0);
        rayToLight.direction = vec4(lightVec, 0.0);
        // Need 'skin' width to avoid hitting itself.
        rayToLight.start += 0.01 * vec4(normalize(rayToLight.direction.xyz), 0);
        HitRecord shadowcastHit;
        shadowcastHit.time = MAX_FLOAT;

        raycast(OBJECT_COUNT, objs, rayToLight, shadowcastHit);

        lightVec = normalize(lightVec);

        vec3 tNormal = fNormal;
        normalView = normalize(tNormal);
        nDotL = dot(normalView, lightVec);

        viewVec = -fPosition;
        viewVec = normalize(viewVec);

        reflectVec = reflect(-lightVec, normalView);
        reflectVec = normalize(reflectVec);

        rDotV = dot(reflectVec, viewVec);
        rDotV = max(rDotV, 0.0f);

        ambient = componentWiseMultiply(hit.mat.ambient, light.ambient);

        // Object cannot directly see the light
        if (shadowcastHit.time >= 1.0 || shadowcastHit.time < 0) {
            diffuse = componentWiseMultiply(hit.mat.diffuse, light.diffuse) * max(nDotL, 0.0);
            if (nDotL > 0)
                specular = componentWiseMultiply(hit.mat.specular, light.specular) * pow(rDotV, max(hit.mat.shininess, 1.0));
        }
        else {
            diffuse = vec3( 0.0, 0.0, 0.0 );
            specular = vec3( 0.0, 0.0, 0.0 );
        }
        absorbColor = absorbColor + ambient + diffuse + specular;
    }

    fColor = absorbColor;

    diffuseColor = vec4(fColor, 1.0);
}

bool intersectsWithBoxSide(inout float tMin, inout float tMax, float start, float dir)
{
    float t1 = (-0.5f - start);
    float t2 = (0.5f - start);
    if (dir == 0) {
        // no intersection
        if (sign(t2) == sign(t1)) return false;

        tMin = -MAX_FLOAT;
        tMax = MAX_FLOAT;
        return true;
    }

    t1 /= dir;
    t2 /= dir;

    if (dir < 0) {
        tMin = min(t1, t2);
        tMax = max(t1, t2);
    }
    else {
        tMin = t1;
        tMax = t2;
    }

    return true;
}