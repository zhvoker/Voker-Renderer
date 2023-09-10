#ifndef RENDERERS_SHADER_H
#define RENDERERS_SHADER_H

#include "../common.h"

class IShader 
{
public:
    mat<2,3,float> varying_uv;  // uv 坐标
    mat<4,3,float> varying_tri; // 裁剪坐标系下的坐标
    mat<3,3,float> varying_nrm; // 法线向量
    mat<3,3,float> ndc_tri;     
    mat<3,3,float> fs_pos; 

    virtual ~IShader();
    virtual Vec4f vertex(int iface, int nthvert) { return Vec4f();};
    virtual bool fragment(const Vec3f& bar, TGAColor &color, Camera &camera) { return false;};
};

IShader::~IShader() {}

mat<3,3,float> TBN(mat<3,3,float> ndc_tri1, mat<2,3,float> varying_uv1, Vec3f bn1)
{
    // 计算TBN矩阵
    mat<3,3,float> A;
    A[0] = ndc_tri1.col(1) - ndc_tri1.col(0);
    A[1] = ndc_tri1.col(2) - ndc_tri1.col(0);
    A[2] = bn1;
    mat<3,3,float> AI = A.invert();
    Vec3f i = AI * Vec3f(varying_uv1[0][1] - varying_uv1[0][0], varying_uv1[0][2] - varying_uv1[0][0], 0);
    Vec3f j = AI * Vec3f(varying_uv1[1][1] - varying_uv1[1][0], varying_uv1[1][2] - varying_uv1[1][0], 0);
    mat<3,3,float> B;
    B.set_col(0, i.normalize());
    B.set_col(1, j.normalize());
    B.set_col(2, bn1);
    return B;
}

class DepthShader : public IShader 
{
public:
    virtual Vec4f vertex(int iface, int nthvert)
    {
        Vec4f gl_Vertex = Projection*ModelView*embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, gl_Vertex);
        ndc_tri.set_col(nthvert, proj<3>(gl_Vertex/gl_Vertex[3]));
        return gl_Vertex;
    }
    virtual bool fragment(Vec3f bar, TGAColor &color, Camera &camera) 
    {
        color = TGAColor(255, 255, 255)*((ndc_tri*bar).z);
        return false;
    }
};

class BlinnPhongShader : public IShader 
{
public:
    virtual Vec4f vertex(int iface, int nthvert) 
    {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        varying_nrm.set_col(nthvert, proj<3>((Projection*ModelView).invert_transpose()*embed<4>(model->normal(iface, nthvert), 0.f)));
        Vec4f gl_Vertex = Projection*ModelView*embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, gl_Vertex);
        // 开启布林冯着色时保存 fs_pos
        if(blinn) fs_pos.set_col(nthvert, proj<3>(gl_Vertex));
        ndc_tri.set_col(nthvert, proj<3>(gl_Vertex/gl_Vertex[3]));
        return gl_Vertex;
    }
    virtual bool fragment(Vec3f bar, TGAColor &color, Camera &camera) 
    {
        Vec4f sb_p = Mshadow*embed<4>(varying_tri*bar); 
        sb_p = sb_p/sb_p[3];
        sb_p =  Viewport*sb_p;
        int idx = int(sb_p[0]) + int(sb_p[1])*width; 
        float shadow = .3+.7*(shadowbuffer[idx]<sb_p[2]); 
        Vec3f bn = (varying_nrm*bar).normalize();
        Vec2f uv = varying_uv*bar;
        Vec3f curPos = fs_pos * bar;
    
        Vec3f viewDir = (proj<3>(Projection*ModelView*embed<4>(camera.eye)) - curPos).normalize();
        Vec3f lightDir = (light_dir - curPos).normalize();

        mat<3,3,float> TBNMatrix = TBN(ndc_tri, varying_uv, bn);
        Vec3f n = (NormalMatrix*TBNMatrix*model->normal(uv)).normalize();
        // 计算光照的衰减(角度)
        float diff = std::max(0.f, n*lightDir);
        // 采样贴图的 颜色
        color = model->diffuse(uv);
        for(int i=0; i<3; i++) color[i] = pow(color[i]/255.0, gamma)*255.0;

        Vec3f r = (n*(n*lightDir*2.f)-lightDir).normalize();   // 计算反射向量
        float spec = 0.f;
        if(blinn){
            Vec3f halfDir = (lightDir + viewDir).normalize();
            spec = pow(std::fmax(n*halfDir, 0.0), 4*model->specular(uv));
        }
        else{
            spec = pow(std::fmax(viewDir*r, 0.0), model->specular(uv));
        }

        for (int i=0; i<3; i++)
        {
            color[i] = pow(std::min<float>(3. + color[i]*shadow*(1.2*diff + .8*spec), 255)/255.0, 1.0/gamma) * 255;
        }
        return false;
    }
};

// PBR 

// 法线分布函数
float DistributionGGX(const float& NdotH, const float& roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float d = 1.0f + (NdotH * NdotH) * (a2 - 1.0);
    return a2 / (PI * d * d);
}
// 菲涅尔方程 
Vec3f FresnelSchlick(const float& HdotV, const Vec3f& F0)
{
    return F0 - (F0 - 1.0f) * pow(Clamp(1.0f - HdotV, 0.0f, 1.0f), 5.0f);
}

// 几何函数
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
float GeometrySmith(const float& NdotV, const float& NdotL, const float& roughness)
{
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// GTR 1
float GTR1(const float& NdotH, const float& alpha)
{
    float a2 = alpha * alpha;
    float cos2Theta = NdotH * NdotH;
    float den = (1.0 + (a2 - 1.0) * cos2Theta);
    return (a2 - 1.0) / (PI * std::log(a2) * den);
}

// GTR 2
float GTR2(const float& NdotH, const float& alpha)
{
    float a2 = alpha * alpha;
    float cos2Theta = NdotH * NdotH;
    float den = (1.0 + (a2 - 1.0) * cos2Theta);
    return a2 / (PI * den * den);
}

Vec3f color2Vec(const TGAColor& color)
{
    return Vec3f(pow(color.bgra[0]/255.0, gamma)*255,pow(color.bgra[1]/255.0, gamma)*255,pow(color.bgra[2]/255.0, gamma)*255);
}

float ACESToneMapping(float color, const float& adapted_lum)
{
    const float A = 2.51f;
    const float B = 0.03f;
    const float C = 2.43f;
    const float D = 0.59f;
    const float E = 0.14f;

    color *= adapted_lum;
    return (color * (A * color + B)) / (color * (C * color + D) + E);
}

class PBRShader : public IShader {
public:
    virtual Vec4f vertex(int iface, int nthvert) 
    {
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        varying_nrm.set_col(nthvert, proj<3>((Projection*ModelView).invert_transpose()*embed<4>(model->normal(iface, nthvert), 0.f)));
        Vec4f gl_Vertex = Projection*ModelView*embed<4>(model->vert(iface, nthvert));
        varying_tri.set_col(nthvert, gl_Vertex);
        ndc_tri.set_col(nthvert, proj<3>(gl_Vertex/gl_Vertex[3]));
        return gl_Vertex;
    }
    
    virtual bool fragment(const Vec3f& bar, TGAColor &color, Camera &camera) 
    {
        Vec3f bn = (varying_nrm*bar).normalize();
        Vec2f uv = varying_uv*bar;
        Vec3f curPos = fs_pos * bar;
        Vec3f viewDir = (proj<3>(Projection*ModelView*embed<4>(camera.eye)) - curPos).normalize();
        mat<3,3,float> TBNMatrix = TBN(ndc_tri, varying_uv, bn);
        Vec3f n = (NormalMatrix*TBNMatrix*model->normal(uv)).normalize();

        float NdotV = std::fmax(0.0f, n * viewDir);
        
        Vec3f F0 = Vec3f(0.04f, 0.04f, 0.04f);
        Vec3f albedo = color2Vec(model->diffuse(uv)); 
        float metallic = model->metallic(uv);
        F0 = Mix(F0, albedo, metallic);

        Vec3f Lo = Vec3f(0.0, 0.0, 0.0);
        for(int i=0; i<4; i++)
        {
            Vec3f lightDir = (light_dirs[i] - curPos).normalize();
            Vec3f halfDir = (lightDir + viewDir).normalize();

            float NdotH = std::fmax(0.0f, n * halfDir);
            float HdotV = std::fmax(0.0f, halfDir * viewDir);
            float NdotL = std::fmax(0.0f, n * lightDir);

            // 计算距离衰减
            float distance = Length(light_dirs[i] - curPos);
            float attenuation = 1.0 / (distance * distance);
            Vec3f radiance = light_colors[i] * attenuation;

            // Cook-Torrance BRDF
            float roughness = model->roughness(uv);
            float NDF = DistributionGGX(NdotH, roughness);
            Vec3f F    = FresnelSchlick(HdotV, F0);
            float G   = GeometrySmith(NdotV, NdotL, roughness);

            Vec3f numerator    = F * NDF * G;
            float denominator = 4.0 * std::fmax(n*viewDir, 0.0) * std::fmax(n*lightDir, 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
            Vec3f specular = numerator / denominator;
            
            Vec3f ks = F;
            Vec3f kd = Vec3f(1.0, 1.0, 1.0) - ks;
            kd = kd * (1.0 - metallic);
            // 计算 irradiance
            Lo += Vecmul((Vecmul(kd, albedo) / PI + specular), radiance) * NdotL;
        }
        // 环境光遮蔽
        float ao = model->ao(uv);
        Vec3f ambient = Vec3f(albedo[0]*0.03, albedo[1]*0.03, albedo[2]*0.03) * ao;
        for(int i=0; i<3; i++)
        {
            color[i] = ambient[i] + Lo[i];
            color[i] = pow(ACESToneMapping(color[i]/255.0, 0.6), 1.0/gamma)*255;

        }
        return false;
    }
};

#endif //RENDERERS_SHADER_H
