// Procedural Sky Shader
// Vertex Shader
#version 330 core

in vec3 vertexPosition;
in vec2 vertexTexCoord;

out vec2 fragTexCoord;
out vec3 worldDir;

uniform mat4 mvp;

void main() {
    fragTexCoord = vertexTexCoord;
    // Convert fullscreen quad UV to world direction
    // UV (0,0) = bottom-left, (1,1) = top-right
    // Map to sphere directions
    float u = vertexTexCoord.x * 2.0 - 1.0;  // -1 to 1
    float v = vertexTexCoord.y * 2.0 - 1.0;  // -1 to 1
    
    // Spherical mapping: u=longitude, v=latitude
    float theta = u * 3.14159265359;      // -PI to PI (longitude)
    float phi = v * 1.57079632679;        // -PI/2 to PI/2 (latitude)
    
    worldDir = vec3(
        cos(phi) * sin(theta),
        sin(phi),
        cos(phi) * cos(theta)
    );
    
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}

// Fragment Shader
#version 330 core

in vec2 fragTexCoord;
in vec3 worldDir;

out vec4 fragColor;

uniform vec3 cameraPos;
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform float sunSize;
uniform float sunIntensity;

uniform vec3 zenithColor;
uniform vec3 horizonColor;
uniform vec3 groundColor;
uniform float atmosphereStrength;

uniform float cloudDensity;
uniform float cloudScale;
uniform float cloudSpeed;
uniform float cloudSoftness;
uniform float skyRotation;
uniform float time;

uniform vec3 fogColor;
uniform float fogDensity;

uniform mat4 viewMatrix;
uniform mat4 projMatrix;

// Simple hash function
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Value noise
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // smoothstep
    
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// FBM (Fractional Brownian Motion) for clouds
float fbm(vec2 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    
    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}

// Simplified sky color calculation
vec3 computeSkyColor(vec3 dir, vec3 sunDir, float time) {
    // Sun direction in world space
    vec3 sun = normalize(sunDir);
    
    // Sun dot product (how much we're looking at the sun)
    float sunDot = max(dot(dir, sun), 0.0);
    
    // Elevation: -1 (down) to 1 (up)
    float elevation = dir.y;
    
    // Sky gradient: zenith -> horizon -> ground
    vec3 skyColor;
    if (elevation > 0.0) {
        // Above horizon: zenith to horizon
        float t = pow(1.0 - elevation, 1.5);
        skyColor = mix(zenithColor, horizonColor, t);
    } else {
        // Below horizon: horizon to ground
        float t = pow(1.0 + elevation, 2.0);
        skyColor = mix(horizonColor, groundColor, t);
    }
    
    // Atmospheric scattering (simplified)
    float scatter = atmosphereStrength * (1.0 - elevation) * 0.5;
    vec3 atmospheric = mix(vec3(0.0), sunColor * 0.3, scatter);
    skyColor += atmospheric;
    
    // Sun disk
    float sunDisk = pow(sunDot, 2048.0) * sunIntensity * 1000.0;
    skyColor += sunColor * sunDisk;
    
    // Sun glow
    float sunGlow = pow(sunDot, 32.0) * sunIntensity * 10.0;
    skyColor += sunColor * sunGlow;
    
    return skyColor;
}

// Cloud noise using rotated coordinates
float cloudNoise(vec2 p, float time, float speed) {
    p += vec2(time * speed * 0.01, 0.0);
    return fbm(p, 5);
}

void main() {
    // Normalize world direction
    vec3 dir = normalize(worldDir);
    
    // Apply sky rotation around Y axis
    float rot = skyRotation * 3.14159265359 / 180.0;
    mat3 rotY = mat3(
        cos(rot), 0.0, sin(rot),
        0.0, 1.0, 0.0,
        -sin(rot), 0.0, cos(rot)
    );
    dir = rotY * dir;
    
    // Base sky color
    vec3 color = computeSkyColor(dir, sunDir, time);
    
    // Procedural clouds
    if (cloudDensity > 0.0) {
        // Cloud coordinates based on world direction
        // Project direction onto XZ plane for cloud mapping
        vec2 cloudCoord = vec2(dir.x, dir.z) * cloudScale * 10.0;
        
        // Add time-based animation
        float cloudTime = time * cloudSpeed * 0.5;
        
        // Multiple cloud layers
        float clouds = 0.0;
        clouds += cloudNoise(cloudCoord * 1.0, cloudTime, 1.0) * 0.5;
        clouds += cloudNoise(cloudCoord * 2.0 + 100.0, cloudTime, 0.5) * 0.25;
        clouds += cloudNoise(cloudCoord * 4.0 - 50.0, cloudTime, 0.25) * 0.125;
        
        // Softness control
        clouds = smoothstep(1.0 - cloudSoftness, 1.0, clouds);
        
        // Only show clouds above horizon (with soft transition)
        float cloudMask = smoothstep(-0.1, 0.15, dir.y);
        clouds *= cloudMask;
        
        // Apply cloud density
        clouds *= cloudDensity;
        
        // Cloud color (white with slight sun tint)
        vec3 cloudColor = vec3(1.0, 0.98, 0.95);
        
        // Blend clouds with sky
        color = mix(color, cloudColor, clouds);
        
        // Add subtle silver lining near sun
        float sunDot = max(dot(normalize(dir), normalize(sunDir)), 0.0);
        float silverLining = pow(sunDot, 16.0) * clouds * 0.3;
        color += sunColor * silverLining;
    }
    
    // Distance fog
    if (fogDensity > 0.0) {
        // For sky, we use a constant "infinite" distance
        // Fog only affects the lower part (near horizon)
        float fogAmount = fogDensity * smoothstep(0.3, -0.2, dir.y);
        color = mix(color, fogColor, fogAmount);
    }
    
    // Tone mapping / gamma correction
    color = pow(color, vec3(1.0 / 2.2));
    color = clamp(color, 0.0, 1.0);
    
    fragColor = vec4(color, 1.0);
}