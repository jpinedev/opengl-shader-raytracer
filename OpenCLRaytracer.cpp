#include "OpenCLRaytracer.hpp"

#include <iostream>
#include <chrono>
#include <stdio.h>

using namespace std;

OpenCLRaytracer::OpenCLRaytracer(const std::vector<ObjectData>& objects, const std::vector<Light>& lights, const std::vector<Ray3D>& rays, const unsigned int MAX_BOUNCES)
    : IRaytracer(objects, lights, rays), MAX_BOUNCES(MAX_BOUNCES), OBJECT_COUNT(objects.size()), LIGHT_COUNT(lights.size()), RAYCAST_COUNT(rays.size())
{
    objArr = new cl_ObjectData[OBJECT_COUNT];
    for (int i = 0; i < OBJECT_COUNT; i++) {
        objArr[i] = cl_ObjectData(objects[i]);
    }

    lightArr = new cl_Light[LIGHT_COUNT];
    for (int i = 0; i < LIGHT_COUNT; i++) {
        lightArr[i] = cl_Light(lights[i]);
    }

    cl_Ray* rayArr = new cl_Ray[RAYCAST_COUNT];

    pixelDataArr = new cl_float3[RAYCAST_COUNT];
    for (int i = 0; i < RAYCAST_COUNT; i++) {
        rayArr[i] = cl_Ray(rays[i]);

        pixelDataArr[i] = { 0.f, 0.f, 0.f };
    }

    // Load the kernel source code into the array source_str
    FILE* fp;
    char* source_str;
    size_t source_size;

    fp = fopen("shade_and_reflect_kernel.cl", "r");
    if (!fp) {
        fprintf(stderr, "Failed to load kernel.\n");
        exit(1);
    }
    source_str = new char[MAX_SOURCE_SIZE];
    source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);

    // Get platform and device information
    cl_platform_id platform_id = NULL;
    cl_device_id device_id = NULL;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
    cl_int ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
    ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1,
        &device_id, &ret_num_devices);

    // Create an OpenCL context
    context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);

    // Create a command queue
    command_queue = clCreateCommandQueueWithProperties(context, device_id, 0, &ret);

    // Create memory buffers on the device for each vector 
    objs_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY,
        OBJECT_COUNT * sizeof(cl_ObjectData), NULL, &ret);
    lights_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY,
        LIGHT_COUNT * sizeof(cl_Light), NULL, &ret);
    rays_mem_obj = clCreateBuffer(context, CL_MEM_READ_ONLY,
        RAYCAST_COUNT * sizeof(cl_Ray), NULL, &ret);
    pixelData_mem_obj = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
        RAYCAST_COUNT * sizeof(cl_float3), NULL, &ret);

    // Create a program from the kernel source
    program = clCreateProgramWithSource(context, 1,
        (const char**)&source_str, (const size_t*)&source_size, &ret);

    // Build the program
    ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
    cl_build_status status;
    ret = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_STATUS, sizeof(cl_build_status), &status, NULL);
    if (ret != 0 || status != CL_BUILD_SUCCESS) {
        cout << ret << endl;

        size_t s;
        ret = clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, MAX_SOURCE_SIZE, source_str, &s);
        cout << source_str;
        throw;
    }

    // Create the OpenCL kernel
    kernel = clCreateKernel(program, "shade_and_reflect", &ret);

    // Set the arguments of the kernel
    ret = clSetKernelArg(kernel, 0, sizeof(cl_uint), &MAX_BOUNCES);
    ret = clSetKernelArg(kernel, 1, sizeof(size_t), &OBJECT_COUNT);
    ret = clSetKernelArg(kernel, 2, sizeof(cl_mem), (void*)&objs_mem_obj);
    ret = clSetKernelArg(kernel, 3, sizeof(size_t), &LIGHT_COUNT);
    ret = clSetKernelArg(kernel, 4, sizeof(cl_mem), (void*)&lights_mem_obj);
    ret = clSetKernelArg(kernel, 5, sizeof(cl_mem), (void*)&rays_mem_obj);
    ret = clSetKernelArg(kernel, 6, sizeof(cl_mem), (void*)&pixelData_mem_obj);

    ret = clEnqueueWriteBuffer(command_queue, objs_mem_obj, CL_TRUE, 0,
        OBJECT_COUNT * sizeof(cl_ObjectData), objArr, 0, NULL, NULL);
    ret = clEnqueueWriteBuffer(command_queue, lights_mem_obj, CL_TRUE, 0,
        LIGHT_COUNT * sizeof(cl_Light), lightArr, 0, NULL, NULL);
    ret = clEnqueueWriteBuffer(command_queue, rays_mem_obj, CL_TRUE, 0,
        RAYCAST_COUNT * sizeof(cl_Ray), rayArr, 0, NULL, NULL);
    ret = clEnqueueWriteBuffer(command_queue, pixelData_mem_obj, CL_TRUE, 0,
        RAYCAST_COUNT * sizeof(cl_float3), pixelDataArr, 0, NULL, NULL);
}

OpenCLRaytracer::~OpenCLRaytracer() {
    cl_int ret;
    ret = clFlush(command_queue);
    ret = clFinish(command_queue);
    ret = clReleaseKernel(kernel);
    ret = clReleaseProgram(program);
    ret = clReleaseMemObject(objs_mem_obj);
    ret = clReleaseMemObject(lights_mem_obj);
    ret = clReleaseMemObject(rays_mem_obj);
    ret = clReleaseMemObject(pixelData_mem_obj);
    ret = clReleaseCommandQueue(command_queue);
    ret = clReleaseContext(context);
    delete[] objArr;
    delete[] lightArr;
    delete[] rayArr;
    delete[] pixelDataArr;
}

void OpenCLRaytracer::Render(std::vector<float>& pixelData)
{
    std::cout << "Executing kernel...\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    // Execute the OpenCL kernel on the list
    size_t global_item_size = RAYCAST_COUNT; // Process the entire lists
    size_t local_item_size = 32; // Divide work items into groups of 64
    cl_int ret = clEnqueueNDRangeKernel(command_queue, kernel, 1, NULL,
        &global_item_size, &local_item_size, 0, NULL, NULL);

    // Read the memory buffer C on the device to the local variable C
    ret = clEnqueueReadBuffer(command_queue, pixelData_mem_obj, CL_TRUE, 0,
        RAYCAST_COUNT * sizeof(cl_float3), pixelDataArr, 0, NULL, NULL);

    auto endTime = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    std::cout << "Kernel finished in " << duration.count() << "ms.\n";

    for (size_t ii = 0; ii < RAYCAST_COUNT; ii++) {
        memcpy(&pixelData[ii * 3], &pixelDataArr[ii], sizeof(float) * 3);
    }
}


inline void cpyVec3ToFloat3(cl_float3* dest, const glm::vec3& src) {
    *dest = { src.x, src.y, src.z };
}
inline void cpyVec4ToFloat4(cl_float4* dest, const glm::vec4& src) {
    *dest = { src.x, src.y, src.z, src.w };
}
inline void cpyMat4ToFloat16(cl_float16* dest, const glm::mat4& src) {
    const float* m = glm::value_ptr(src);
    *dest = { m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15] };
}

OpenCLRaytracer::cl_Ray::cl_Ray() : start({ 0, 0, 0 }), direction({ 0,0,0 }) { }
OpenCLRaytracer::cl_Ray::cl_Ray(const Ray3D& cpy) {
    cpyVec4ToFloat4(&start, cpy.start);
    cpyVec4ToFloat4(&direction, cpy.direction);
}

OpenCLRaytracer::cl_Material::cl_Material() : ambient({ 0., 0., 0. }), diffuse(ambient), specular(ambient), absorption(1), reflection(0), transparency(0), shininess(1) { }
OpenCLRaytracer::cl_Material::cl_Material(const Material& cpy) : absorption(cpy.absorption), reflection(cpy.reflection), transparency(cpy.transparency), shininess(cpy.shininess) {
    cpyVec3ToFloat3(&ambient, cpy.ambient);
    cpyVec3ToFloat3(&diffuse, cpy.diffuse);
    cpyVec3ToFloat3(&specular, cpy.specular);
}

OpenCLRaytracer::cl_ObjectData::cl_ObjectData() : mv({ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 }), mvInverse(mv), mvInverseTranspose(mv), type(0) { }
OpenCLRaytracer::cl_ObjectData::cl_ObjectData(const ObjectData& cpy) : mat(cpy.mat) {
    cpyMat4ToFloat16(&mv, cpy.mv);
    cpyMat4ToFloat16(&mvInverse, cpy.mvInverse);
    cpyMat4ToFloat16(&mvInverseTranspose, cpy.mvInverseTranspose);
    type = (cl_uint)cpy.type;
}

OpenCLRaytracer::cl_Light::cl_Light() : ambient({ 0., 0., 0. }), diffuse(ambient), specular(ambient), position({ 0., 0., 0., 1. }) { }
OpenCLRaytracer::cl_Light::cl_Light(const Light& cpy) {
    cpyVec3ToFloat3(&ambient, cpy.ambient);
    cpyVec3ToFloat3(&diffuse, cpy.diffuse);
    cpyVec3ToFloat3(&specular, cpy.specular);
    cpyVec4ToFloat4(&position, cpy.lightPosition);
}
