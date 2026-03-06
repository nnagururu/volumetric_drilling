uniform vec3 uMinCorner;
uniform vec3 uMaxCorner;
uniform vec3 uTextureScale;
uniform vec3 uGradientDelta;
uniform sampler3D uVolume;
uniform float uIsosurface;
uniform float uResolution;

varying vec4 vPosition;
uniform bool uSmoothVolume;
uniform int uSmoothingLevel;
uniform float uAlphaThreshold;
uniform bool uEnableDVR;

vec3 dx = vec3(uGradientDelta.x, 0.0, 0.0);
vec3 dy = vec3(0.0, uGradientDelta.y, 0.0);
vec3 dz = vec3(0.0, 0.0, uGradientDelta.z);


//----------------------------------------------------------------------
// Finds the entering intersection between a ray e1+d and the volume's
// bounding box.
//----------------------------------------------------------------------

float entry(vec3 e1, vec3 d)
{
    float t = distance(uMinCorner, uMaxCorner);

    vec3 a = (uMinCorner - e1) / d;
    vec3 b = (uMaxCorner - e1) / d;
    vec3 u = min(a, b);

    return max( max(-t, u.x), max(u.y, u.z) );
}


float isoValue(vec3 tc){
  return texture3D(uVolume, tc).a;
}

float ambientOcclusion(vec3 tc, float step_size) {
    float ao = 0.0;
    int nSamples = 8;
    float stepSize = step_size;

    // Sample directions (you can use a spherical Fibonacci pattern for better coverage)
    vec3 dirs[8];
    dirs[0] = vec3(1, 0, 0);
    dirs[1] = vec3(-1, 0, 0);
    dirs[2] = vec3(0, 1, 0);
    dirs[3] = vec3(0, -1, 0);
    dirs[4] = vec3(0, 0, 1);
    dirs[5] = vec3(0, 0, -1);
    dirs[6] = normalize(vec3(1,1,1));
    dirs[7] = normalize(vec3(-1,-1,-1));

    for (int i = 0; i < nSamples; ++i) {
        vec3 sampleTC = tc + stepSize * dirs[i];
        float density = texture3D(uVolume, sampleTC).a;
        ao += density;
    }

    ao /= float(nSamples);  // normalize
    ao = 1.0 - ao;          // invert: higher density = more occlusion

    return clamp(ao, 0.0, 1.0);
}

float gaussianWeight(int x, int y, int z) {
    float sigma = 1.0;
    float r2 = float(x*x + y*y + z*z);
    return exp(-r2 / (2.0 * sigma * sigma));
}

vec3 gaussianSmoothedGradient(vec3 tc, int level) {
    vec3 nabla = vec3(0.0);
    float weightSum = 0.0;

    for (int x = -level; x <= level; ++x)
    for (int y = -level; y <= level; ++y)
    for (int z = -level; z <= level; ++z) {
        vec3 offset = vec3(float(x), float(y), float(z)) * uGradientDelta;
        vec3 sampleTC = tc + offset;

        // Central difference at this offset
        vec3 g = vec3(
            isoValue(sampleTC + dx) - isoValue(sampleTC - dx),
            isoValue(sampleTC + dy) - isoValue(sampleTC - dy),
            isoValue(sampleTC + dz) - isoValue(sampleTC - dz)
        );

        float w = gaussianWeight(x, y, z);
        nabla += w * g;
        weightSum += w;
    }

    return (nabla / weightSum) * uTextureScale;
}

vec3 gradientSmooth(vec3 tc, int level)
{
    vec3 nabla;
    for (int i = 1 ; i <= level ; i++){
        for (int j = 1 ; j <= level ; j++){
            for (int k = 1 ; k <= level ; k++){
                nabla[0] += (isoValue(tc + float(i) * dx) - isoValue(tc - float(i) * dx));
                nabla[1] += (isoValue(tc + float(j) * dy) - isoValue(tc - float(j) * dy));
                nabla[2] += (isoValue(tc + float(k) * dz) - isoValue(tc - float(k) * dz));
            }
        }
    }

    return (nabla / uGradientDelta) * uTextureScale;
}

//----------------------------------------------------------------------
// Estimates the intensity gradient of the volume in model space
//----------------------------------------------------------------------

vec3 gradient(vec3 tc)
{
    vec3 nabla = vec3(
        isoValue(tc + dx) - isoValue(tc - dx),
        isoValue(tc + dy) - isoValue(tc - dy),
        isoValue(tc + dz) - isoValue(tc - dz)
    );

    return (nabla / uGradientDelta) * uTextureScale;
}


//----------------------------------------------------------------------
//  Performs interval bisection and returns the value between a and b
//  closest to isosurface. When s(b) > s(a), direction should be +1.0,
//  and -1.0 otherwise.
//----------------------------------------------------------------------

vec3 refine(vec3 a, vec3 b, float isosurface, float direction)
{
    for (int i = 0; i < 6; ++i)
    {
        vec3 m = 0.5 * (a + b);
        float v = (texture3D(uVolume, m).a - isosurface) * direction;
        if (v >= 0.0)   b = m;
        else            a = m;
    }
    return b;
}


//----------------------------------------------------------------------
//  Computes phong shading based on current light and material
//  properties.
//----------------------------------------------------------------------

vec3 shade(vec3 p, vec3 v, vec3 n)
{
    vec4 lp = gl_ModelViewMatrixInverse * gl_LightSource[0].position;
    vec3 l = normalize(lp.xyz - p * lp.w);
    vec3 h = normalize(l+v);
    float cos_i = max(dot(n, l), 0.0);
    float cos_h = max(dot(n, h), 0.0);

    vec3 Ia = gl_FrontLightProduct[0].ambient.rgb;
    vec3 Id = gl_FrontLightProduct[0].diffuse.rgb * cos_i;
    vec3 Is = gl_FrontLightProduct[0].specular.rgb * pow(cos_h, gl_FrontMaterial.shininess);

    return (Ia + Id + Is);
}


//----------------------------------------------------------------------
//  Main fragment shader code.
//----------------------------------------------------------------------

void main(void)
{
    vec4 camera = gl_ModelViewMatrixInverse * vec4(0.0, 0.0, 0.0, 1.0);
    vec3 raydir = normalize(vPosition.xyz - camera.xyz);

    float t_entry = entry(vPosition.xyz, raydir);
    t_entry = max(t_entry, -distance(camera.xyz, vPosition.xyz));
    
    uResolution = 400; // Jonathan test

    // estimate a reasonable step size
    float t_step = distance(uMinCorner, uMaxCorner) / uResolution;
    
    // Performance: Use larger steps for DVR
    if (uEnableDVR) {
        t_step *= 1.25;  // Adjust this value: Higher is worse quality but faster performance
    }

    vec3 tc_step = uTextureScale * (t_step * raydir);

    // cast the ray (in model space)
    vec4 sum = vec4(0.0);
    vec3 tc = gl_TexCoord[0].stp + t_entry * tc_step / t_step;
    float alpha_acc = 0.0;

    bool smoothVolume = false;
    bool firstHit = false;
    vec3 surfacePosition;

    for (float t = t_entry; t < 0.0; t += t_step, tc += tc_step)
    {
        float intensity = isoValue(tc);
        if (uEnableDVR) {
            // Skip empty space
            if (intensity < 0.02) {
                continue;
            }
            
            // Simple transfer function
            float alpha = clamp(intensity * 0.3, 0.0, 1.0);
            
            // Track first significant contribution for depth
            if (!firstHit && alpha > 0.05) {
                firstHit = true;
                vec4 clip = gl_ModelViewProjectionMatrix * vec4(vPosition.xyz + t * raydir, 1.0);
                gl_FragDepth = (gl_DepthRange.diff * clip.z / clip.w + gl_DepthRange.near + gl_DepthRange.far) * 0.5;
            }
            
            // Quick gradient for lighting
            vec3 nabla = gradient(tc);
            vec3 normal = -normalize(nabla);
            vec3 view = -raydir;
            
            // Get color from volume
            vec3 texColor = texture3D(uVolume, tc).rgb;
            
            // Simple lighting
            vec3 litColor = shade(vPosition.xyz + t * raydir, view, normal) * texColor * 3.2;
            
            // Composite
            litColor *= alpha;
            sum.rgb += (1.0 - alpha_acc) * litColor;
            alpha_acc += (1.0 - alpha_acc) * alpha;
            
            // Early termination
            if (alpha_acc > 0.95) {
                break;
            }
        }
        else{
            if (intensity > uIsosurface){
                vec3 tcr = refine(tc - tc_step, tc, uIsosurface, 1.0);

                vec3 nabla = gradient(tcr);

                float dt = length(tcr - tc) / length(tc_step);
                vec3 position = vPosition.xyz + (t - dt * t_step) * raydir;
                vec3 normal = -normalize(nabla);
                vec3 view = -raydir;
                vec3 colour = shade(position, view, normal) * texture3D(uVolume, tcr).rgb / uIsosurface;
                sum = vec4(colour, 1.0);
                alpha_acc = 1.0;

                // calculate fragment depth
                vec4 clip = gl_ModelViewProjectionMatrix * vec4(position, 1.0);
                gl_FragDepth = (gl_DepthRange.diff * clip.z / clip.w + gl_DepthRange.near + gl_DepthRange.far) * 0.5;

                break;
            }
        }
    }

    sum.a = alpha_acc;
    // discard the fragment if no geometry was intersected
    if (sum.a <= 0.0) discard;

    
    gl_FragColor = sum;
}
