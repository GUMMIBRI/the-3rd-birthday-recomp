/* SPDX-License-Identifier: GPL-2.0-or-later */
#define SR_VK_DISPATCH 1
#define SR_VK_DISPATCH_IMPLEMENTATION 1
#include "vk_dispatch.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdio.h>
#include <string.h>

SrVkDispatch sr_vk;
static PFN_vkGetInstanceProcAddr get_instance_proc;
static VkInstance loaded_instance;
static VkDevice loaded_device;
static int instance_ready;

static int missing(const char *stage, const char *name) {
    fprintf(stderr, "Vulkan dispatch: missing %s function %s\n", stage, name);
    SDL_SetError("Vulkan dispatch: missing %s function %s", stage, name);
    return 0;
}
static void clear_device(void) {
    sr_vk.vkAcquireNextImageKHR = NULL;
    sr_vk.vkAllocateCommandBuffers = NULL;
    sr_vk.vkAllocateDescriptorSets = NULL;
    sr_vk.vkAllocateMemory = NULL;
    sr_vk.vkBeginCommandBuffer = NULL;
    sr_vk.vkBindBufferMemory = NULL;
    sr_vk.vkBindImageMemory = NULL;
    sr_vk.vkCmdBeginRenderPass = NULL;
    sr_vk.vkCmdBindDescriptorSets = NULL;
    sr_vk.vkCmdBindPipeline = NULL;
    sr_vk.vkCmdBindVertexBuffers = NULL;
    sr_vk.vkCmdBlitImage = NULL;
    sr_vk.vkCmdClearColorImage = NULL;
    sr_vk.vkCmdClearDepthStencilImage = NULL;
    sr_vk.vkCmdCopyBufferToImage = NULL;
    sr_vk.vkCmdCopyImage = NULL;
    sr_vk.vkCmdCopyImageToBuffer = NULL;
    sr_vk.vkCmdDraw = NULL;
    sr_vk.vkCmdEndRenderPass = NULL;
    sr_vk.vkCmdPipelineBarrier = NULL;
    sr_vk.vkCmdPushConstants = NULL;
    sr_vk.vkCmdSetBlendConstants = NULL;
    sr_vk.vkCmdSetScissor = NULL;
    sr_vk.vkCmdSetViewport = NULL;
    sr_vk.vkCreateBuffer = NULL;
    sr_vk.vkCreateCommandPool = NULL;
    sr_vk.vkCreateDescriptorPool = NULL;
    sr_vk.vkCreateDescriptorSetLayout = NULL;
    sr_vk.vkCreateFence = NULL;
    sr_vk.vkCreateFramebuffer = NULL;
    sr_vk.vkCreateGraphicsPipelines = NULL;
    sr_vk.vkCreateImage = NULL;
    sr_vk.vkCreateImageView = NULL;
    sr_vk.vkCreatePipelineLayout = NULL;
    sr_vk.vkCreateRenderPass = NULL;
    sr_vk.vkCreateSampler = NULL;
    sr_vk.vkCreateSemaphore = NULL;
    sr_vk.vkCreateShaderModule = NULL;
    sr_vk.vkCreateSwapchainKHR = NULL;
    sr_vk.vkDestroyBuffer = NULL;
    sr_vk.vkDestroyCommandPool = NULL;
    sr_vk.vkDestroyDevice = NULL;
    sr_vk.vkDestroyFence = NULL;
    sr_vk.vkDestroyFramebuffer = NULL;
    sr_vk.vkDestroyImage = NULL;
    sr_vk.vkDestroyImageView = NULL;
    sr_vk.vkDestroySampler = NULL;
    sr_vk.vkDestroySemaphore = NULL;
    sr_vk.vkDestroySwapchainKHR = NULL;
    sr_vk.vkDeviceWaitIdle = NULL;
    sr_vk.vkEndCommandBuffer = NULL;
    sr_vk.vkFreeDescriptorSets = NULL;
    sr_vk.vkFreeMemory = NULL;
    sr_vk.vkGetBufferMemoryRequirements = NULL;
    sr_vk.vkGetDeviceQueue = NULL;
    sr_vk.vkGetImageMemoryRequirements = NULL;
    sr_vk.vkGetSwapchainImagesKHR = NULL;
    sr_vk.vkMapMemory = NULL;
    sr_vk.vkQueuePresentKHR = NULL;
    sr_vk.vkQueueSubmit = NULL;
    sr_vk.vkQueueWaitIdle = NULL;
    sr_vk.vkResetCommandBuffer = NULL;
    sr_vk.vkResetFences = NULL;
    sr_vk.vkUpdateDescriptorSets = NULL;
    sr_vk.vkWaitForFences = NULL;
    loaded_device = VK_NULL_HANDLE;
}
static void clear_instance(void) {
    clear_device();
    sr_vk.vkCreateDevice = NULL;
    sr_vk.vkDestroyInstance = NULL;
    sr_vk.vkDestroySurfaceKHR = NULL;
    sr_vk.vkEnumeratePhysicalDevices = NULL;
    sr_vk.vkGetPhysicalDeviceFeatures = NULL;
    sr_vk.vkGetPhysicalDeviceMemoryProperties = NULL;
    sr_vk.vkGetPhysicalDeviceProperties = NULL;
    sr_vk.vkGetPhysicalDeviceQueueFamilyProperties = NULL;
    sr_vk.vkGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
    sr_vk.vkGetPhysicalDeviceSurfaceFormatsKHR = NULL;
    sr_vk.vkGetPhysicalDeviceSurfacePresentModesKHR = NULL;
    sr_vk.vkGetPhysicalDeviceSurfaceSupportKHR = NULL;
    sr_vk.vkGetDeviceProcAddr = NULL;
    loaded_instance = VK_NULL_HANDLE;
    instance_ready = 0;
}
void sr_vk_reset(void) {
    memset(&sr_vk, 0, sizeof(sr_vk));
    get_instance_proc = NULL;
    loaded_instance = VK_NULL_HANDLE;
    loaded_device = VK_NULL_HANDLE;
    instance_ready = 0;
}
int sr_vk_load_global(void) {
    sr_vk_reset();
    get_instance_proc = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
    if (!get_instance_proc) return missing("global", "vkGetInstanceProcAddr (SDL)");
    sr_vk.vkCreateInstance = (PFN_vkCreateInstance)get_instance_proc(VK_NULL_HANDLE, "vkCreateInstance");
    if (!sr_vk.vkCreateInstance) return missing("global", "vkCreateInstance");
    return 1;
}
int sr_vk_load_instance(VkInstance instance) {
    int ready = 1;
    clear_instance();
    if (!get_instance_proc || !sr_vk.vkCreateInstance || !instance)
        return missing("instance", "initialized global dispatch and non-null instance");
    loaded_instance = instance;
    sr_vk.vkCreateDevice = (PFN_vkCreateDevice)get_instance_proc(instance, "vkCreateDevice");
    if (!sr_vk.vkCreateDevice) { if (ready) missing("instance", "vkCreateDevice"); ready = 0; }
    sr_vk.vkDestroyInstance = (PFN_vkDestroyInstance)get_instance_proc(instance, "vkDestroyInstance");
    if (!sr_vk.vkDestroyInstance) { if (ready) missing("instance", "vkDestroyInstance"); ready = 0; }
    sr_vk.vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)get_instance_proc(instance, "vkDestroySurfaceKHR");
    if (!sr_vk.vkDestroySurfaceKHR) { if (ready) missing("instance", "vkDestroySurfaceKHR"); ready = 0; }
    sr_vk.vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)get_instance_proc(instance, "vkEnumeratePhysicalDevices");
    if (!sr_vk.vkEnumeratePhysicalDevices) { if (ready) missing("instance", "vkEnumeratePhysicalDevices"); ready = 0; }
    sr_vk.vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)get_instance_proc(instance, "vkGetPhysicalDeviceFeatures");
    if (!sr_vk.vkGetPhysicalDeviceFeatures) { if (ready) missing("instance", "vkGetPhysicalDeviceFeatures"); ready = 0; }
    sr_vk.vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)get_instance_proc(instance, "vkGetPhysicalDeviceMemoryProperties");
    if (!sr_vk.vkGetPhysicalDeviceMemoryProperties) { if (ready) missing("instance", "vkGetPhysicalDeviceMemoryProperties"); ready = 0; }
    sr_vk.vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)get_instance_proc(instance, "vkGetPhysicalDeviceProperties");
    if (!sr_vk.vkGetPhysicalDeviceProperties) { if (ready) missing("instance", "vkGetPhysicalDeviceProperties"); ready = 0; }
    sr_vk.vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)get_instance_proc(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    if (!sr_vk.vkGetPhysicalDeviceQueueFamilyProperties) { if (ready) missing("instance", "vkGetPhysicalDeviceQueueFamilyProperties"); ready = 0; }
    sr_vk.vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)get_instance_proc(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    if (!sr_vk.vkGetPhysicalDeviceSurfaceCapabilitiesKHR) { if (ready) missing("instance", "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"); ready = 0; }
    sr_vk.vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)get_instance_proc(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    if (!sr_vk.vkGetPhysicalDeviceSurfaceFormatsKHR) { if (ready) missing("instance", "vkGetPhysicalDeviceSurfaceFormatsKHR"); ready = 0; }
    sr_vk.vkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)get_instance_proc(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    if (!sr_vk.vkGetPhysicalDeviceSurfacePresentModesKHR) { if (ready) missing("instance", "vkGetPhysicalDeviceSurfacePresentModesKHR"); ready = 0; }
    sr_vk.vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)get_instance_proc(instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
    if (!sr_vk.vkGetPhysicalDeviceSurfaceSupportKHR) { if (ready) missing("instance", "vkGetPhysicalDeviceSurfaceSupportKHR"); ready = 0; }
    sr_vk.vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)get_instance_proc(instance, "vkGetDeviceProcAddr");
    if (!sr_vk.vkGetDeviceProcAddr) { if (ready) missing("instance", "vkGetDeviceProcAddr"); ready = 0; }
    instance_ready = ready;
    return ready;
}
int sr_vk_load_device(VkDevice device) {
    int ready = 1;
    clear_device();
    if (!instance_ready || !loaded_instance || !sr_vk.vkGetDeviceProcAddr || !device)
        return missing("device", "initialized instance dispatch and non-null device");
    loaded_device = device;
    sr_vk.vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)sr_vk.vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR");
    if (!sr_vk.vkAcquireNextImageKHR) { if (ready) missing("device", "vkAcquireNextImageKHR"); ready = 0; }
    sr_vk.vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)sr_vk.vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers");
    if (!sr_vk.vkAllocateCommandBuffers) { if (ready) missing("device", "vkAllocateCommandBuffers"); ready = 0; }
    sr_vk.vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)sr_vk.vkGetDeviceProcAddr(device, "vkAllocateDescriptorSets");
    if (!sr_vk.vkAllocateDescriptorSets) { if (ready) missing("device", "vkAllocateDescriptorSets"); ready = 0; }
    sr_vk.vkAllocateMemory = (PFN_vkAllocateMemory)sr_vk.vkGetDeviceProcAddr(device, "vkAllocateMemory");
    if (!sr_vk.vkAllocateMemory) { if (ready) missing("device", "vkAllocateMemory"); ready = 0; }
    sr_vk.vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)sr_vk.vkGetDeviceProcAddr(device, "vkBeginCommandBuffer");
    if (!sr_vk.vkBeginCommandBuffer) { if (ready) missing("device", "vkBeginCommandBuffer"); ready = 0; }
    sr_vk.vkBindBufferMemory = (PFN_vkBindBufferMemory)sr_vk.vkGetDeviceProcAddr(device, "vkBindBufferMemory");
    if (!sr_vk.vkBindBufferMemory) { if (ready) missing("device", "vkBindBufferMemory"); ready = 0; }
    sr_vk.vkBindImageMemory = (PFN_vkBindImageMemory)sr_vk.vkGetDeviceProcAddr(device, "vkBindImageMemory");
    if (!sr_vk.vkBindImageMemory) { if (ready) missing("device", "vkBindImageMemory"); ready = 0; }
    sr_vk.vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)sr_vk.vkGetDeviceProcAddr(device, "vkCmdBeginRenderPass");
    if (!sr_vk.vkCmdBeginRenderPass) { if (ready) missing("device", "vkCmdBeginRenderPass"); ready = 0; }
    sr_vk.vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)sr_vk.vkGetDeviceProcAddr(device, "vkCmdBindDescriptorSets");
    if (!sr_vk.vkCmdBindDescriptorSets) { if (ready) missing("device", "vkCmdBindDescriptorSets"); ready = 0; }
    sr_vk.vkCmdBindPipeline = (PFN_vkCmdBindPipeline)sr_vk.vkGetDeviceProcAddr(device, "vkCmdBindPipeline");
    if (!sr_vk.vkCmdBindPipeline) { if (ready) missing("device", "vkCmdBindPipeline"); ready = 0; }
    sr_vk.vkCmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)sr_vk.vkGetDeviceProcAddr(device, "vkCmdBindVertexBuffers");
    if (!sr_vk.vkCmdBindVertexBuffers) { if (ready) missing("device", "vkCmdBindVertexBuffers"); ready = 0; }
    sr_vk.vkCmdBlitImage = (PFN_vkCmdBlitImage)sr_vk.vkGetDeviceProcAddr(device, "vkCmdBlitImage");
    if (!sr_vk.vkCmdBlitImage) { if (ready) missing("device", "vkCmdBlitImage"); ready = 0; }
    sr_vk.vkCmdClearColorImage = (PFN_vkCmdClearColorImage)sr_vk.vkGetDeviceProcAddr(device, "vkCmdClearColorImage");
    if (!sr_vk.vkCmdClearColorImage) { if (ready) missing("device", "vkCmdClearColorImage"); ready = 0; }
    sr_vk.vkCmdClearDepthStencilImage = (PFN_vkCmdClearDepthStencilImage)sr_vk.vkGetDeviceProcAddr(device, "vkCmdClearDepthStencilImage");
    if (!sr_vk.vkCmdClearDepthStencilImage) { if (ready) missing("device", "vkCmdClearDepthStencilImage"); ready = 0; }
    sr_vk.vkCmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage)sr_vk.vkGetDeviceProcAddr(device, "vkCmdCopyBufferToImage");
    if (!sr_vk.vkCmdCopyBufferToImage) { if (ready) missing("device", "vkCmdCopyBufferToImage"); ready = 0; }
    sr_vk.vkCmdCopyImage = (PFN_vkCmdCopyImage)sr_vk.vkGetDeviceProcAddr(device, "vkCmdCopyImage");
    if (!sr_vk.vkCmdCopyImage) { if (ready) missing("device", "vkCmdCopyImage"); ready = 0; }
    sr_vk.vkCmdCopyImageToBuffer = (PFN_vkCmdCopyImageToBuffer)sr_vk.vkGetDeviceProcAddr(device, "vkCmdCopyImageToBuffer");
    if (!sr_vk.vkCmdCopyImageToBuffer) { if (ready) missing("device", "vkCmdCopyImageToBuffer"); ready = 0; }
    sr_vk.vkCmdDraw = (PFN_vkCmdDraw)sr_vk.vkGetDeviceProcAddr(device, "vkCmdDraw");
    if (!sr_vk.vkCmdDraw) { if (ready) missing("device", "vkCmdDraw"); ready = 0; }
    sr_vk.vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)sr_vk.vkGetDeviceProcAddr(device, "vkCmdEndRenderPass");
    if (!sr_vk.vkCmdEndRenderPass) { if (ready) missing("device", "vkCmdEndRenderPass"); ready = 0; }
    sr_vk.vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)sr_vk.vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier");
    if (!sr_vk.vkCmdPipelineBarrier) { if (ready) missing("device", "vkCmdPipelineBarrier"); ready = 0; }
    sr_vk.vkCmdPushConstants = (PFN_vkCmdPushConstants)sr_vk.vkGetDeviceProcAddr(device, "vkCmdPushConstants");
    if (!sr_vk.vkCmdPushConstants) { if (ready) missing("device", "vkCmdPushConstants"); ready = 0; }
    sr_vk.vkCmdSetBlendConstants = (PFN_vkCmdSetBlendConstants)sr_vk.vkGetDeviceProcAddr(device, "vkCmdSetBlendConstants");
    if (!sr_vk.vkCmdSetBlendConstants) { if (ready) missing("device", "vkCmdSetBlendConstants"); ready = 0; }
    sr_vk.vkCmdSetScissor = (PFN_vkCmdSetScissor)sr_vk.vkGetDeviceProcAddr(device, "vkCmdSetScissor");
    if (!sr_vk.vkCmdSetScissor) { if (ready) missing("device", "vkCmdSetScissor"); ready = 0; }
    sr_vk.vkCmdSetViewport = (PFN_vkCmdSetViewport)sr_vk.vkGetDeviceProcAddr(device, "vkCmdSetViewport");
    if (!sr_vk.vkCmdSetViewport) { if (ready) missing("device", "vkCmdSetViewport"); ready = 0; }
    sr_vk.vkCreateBuffer = (PFN_vkCreateBuffer)sr_vk.vkGetDeviceProcAddr(device, "vkCreateBuffer");
    if (!sr_vk.vkCreateBuffer) { if (ready) missing("device", "vkCreateBuffer"); ready = 0; }
    sr_vk.vkCreateCommandPool = (PFN_vkCreateCommandPool)sr_vk.vkGetDeviceProcAddr(device, "vkCreateCommandPool");
    if (!sr_vk.vkCreateCommandPool) { if (ready) missing("device", "vkCreateCommandPool"); ready = 0; }
    sr_vk.vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool)sr_vk.vkGetDeviceProcAddr(device, "vkCreateDescriptorPool");
    if (!sr_vk.vkCreateDescriptorPool) { if (ready) missing("device", "vkCreateDescriptorPool"); ready = 0; }
    sr_vk.vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)sr_vk.vkGetDeviceProcAddr(device, "vkCreateDescriptorSetLayout");
    if (!sr_vk.vkCreateDescriptorSetLayout) { if (ready) missing("device", "vkCreateDescriptorSetLayout"); ready = 0; }
    sr_vk.vkCreateFence = (PFN_vkCreateFence)sr_vk.vkGetDeviceProcAddr(device, "vkCreateFence");
    if (!sr_vk.vkCreateFence) { if (ready) missing("device", "vkCreateFence"); ready = 0; }
    sr_vk.vkCreateFramebuffer = (PFN_vkCreateFramebuffer)sr_vk.vkGetDeviceProcAddr(device, "vkCreateFramebuffer");
    if (!sr_vk.vkCreateFramebuffer) { if (ready) missing("device", "vkCreateFramebuffer"); ready = 0; }
    sr_vk.vkCreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)sr_vk.vkGetDeviceProcAddr(device, "vkCreateGraphicsPipelines");
    if (!sr_vk.vkCreateGraphicsPipelines) { if (ready) missing("device", "vkCreateGraphicsPipelines"); ready = 0; }
    sr_vk.vkCreateImage = (PFN_vkCreateImage)sr_vk.vkGetDeviceProcAddr(device, "vkCreateImage");
    if (!sr_vk.vkCreateImage) { if (ready) missing("device", "vkCreateImage"); ready = 0; }
    sr_vk.vkCreateImageView = (PFN_vkCreateImageView)sr_vk.vkGetDeviceProcAddr(device, "vkCreateImageView");
    if (!sr_vk.vkCreateImageView) { if (ready) missing("device", "vkCreateImageView"); ready = 0; }
    sr_vk.vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout)sr_vk.vkGetDeviceProcAddr(device, "vkCreatePipelineLayout");
    if (!sr_vk.vkCreatePipelineLayout) { if (ready) missing("device", "vkCreatePipelineLayout"); ready = 0; }
    sr_vk.vkCreateRenderPass = (PFN_vkCreateRenderPass)sr_vk.vkGetDeviceProcAddr(device, "vkCreateRenderPass");
    if (!sr_vk.vkCreateRenderPass) { if (ready) missing("device", "vkCreateRenderPass"); ready = 0; }
    sr_vk.vkCreateSampler = (PFN_vkCreateSampler)sr_vk.vkGetDeviceProcAddr(device, "vkCreateSampler");
    if (!sr_vk.vkCreateSampler) { if (ready) missing("device", "vkCreateSampler"); ready = 0; }
    sr_vk.vkCreateSemaphore = (PFN_vkCreateSemaphore)sr_vk.vkGetDeviceProcAddr(device, "vkCreateSemaphore");
    if (!sr_vk.vkCreateSemaphore) { if (ready) missing("device", "vkCreateSemaphore"); ready = 0; }
    sr_vk.vkCreateShaderModule = (PFN_vkCreateShaderModule)sr_vk.vkGetDeviceProcAddr(device, "vkCreateShaderModule");
    if (!sr_vk.vkCreateShaderModule) { if (ready) missing("device", "vkCreateShaderModule"); ready = 0; }
    sr_vk.vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)sr_vk.vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR");
    if (!sr_vk.vkCreateSwapchainKHR) { if (ready) missing("device", "vkCreateSwapchainKHR"); ready = 0; }
    sr_vk.vkDestroyBuffer = (PFN_vkDestroyBuffer)sr_vk.vkGetDeviceProcAddr(device, "vkDestroyBuffer");
    if (!sr_vk.vkDestroyBuffer) { if (ready) missing("device", "vkDestroyBuffer"); ready = 0; }
    sr_vk.vkDestroyCommandPool = (PFN_vkDestroyCommandPool)sr_vk.vkGetDeviceProcAddr(device, "vkDestroyCommandPool");
    if (!sr_vk.vkDestroyCommandPool) { if (ready) missing("device", "vkDestroyCommandPool"); ready = 0; }
    sr_vk.vkDestroyDevice = (PFN_vkDestroyDevice)sr_vk.vkGetDeviceProcAddr(device, "vkDestroyDevice");
    if (!sr_vk.vkDestroyDevice) { if (ready) missing("device", "vkDestroyDevice"); ready = 0; }
    sr_vk.vkDestroyFence = (PFN_vkDestroyFence)sr_vk.vkGetDeviceProcAddr(device, "vkDestroyFence");
    if (!sr_vk.vkDestroyFence) { if (ready) missing("device", "vkDestroyFence"); ready = 0; }
    sr_vk.vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer)sr_vk.vkGetDeviceProcAddr(device, "vkDestroyFramebuffer");
    if (!sr_vk.vkDestroyFramebuffer) { if (ready) missing("device", "vkDestroyFramebuffer"); ready = 0; }
    sr_vk.vkDestroyImage = (PFN_vkDestroyImage)sr_vk.vkGetDeviceProcAddr(device, "vkDestroyImage");
    if (!sr_vk.vkDestroyImage) { if (ready) missing("device", "vkDestroyImage"); ready = 0; }
    sr_vk.vkDestroyImageView = (PFN_vkDestroyImageView)sr_vk.vkGetDeviceProcAddr(device, "vkDestroyImageView");
    if (!sr_vk.vkDestroyImageView) { if (ready) missing("device", "vkDestroyImageView"); ready = 0; }
    sr_vk.vkDestroySampler = (PFN_vkDestroySampler)sr_vk.vkGetDeviceProcAddr(device, "vkDestroySampler");
    if (!sr_vk.vkDestroySampler) { if (ready) missing("device", "vkDestroySampler"); ready = 0; }
    sr_vk.vkDestroySemaphore = (PFN_vkDestroySemaphore)sr_vk.vkGetDeviceProcAddr(device, "vkDestroySemaphore");
    if (!sr_vk.vkDestroySemaphore) { if (ready) missing("device", "vkDestroySemaphore"); ready = 0; }
    sr_vk.vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)sr_vk.vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR");
    if (!sr_vk.vkDestroySwapchainKHR) { if (ready) missing("device", "vkDestroySwapchainKHR"); ready = 0; }
    sr_vk.vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)sr_vk.vkGetDeviceProcAddr(device, "vkDeviceWaitIdle");
    if (!sr_vk.vkDeviceWaitIdle) { if (ready) missing("device", "vkDeviceWaitIdle"); ready = 0; }
    sr_vk.vkEndCommandBuffer = (PFN_vkEndCommandBuffer)sr_vk.vkGetDeviceProcAddr(device, "vkEndCommandBuffer");
    if (!sr_vk.vkEndCommandBuffer) { if (ready) missing("device", "vkEndCommandBuffer"); ready = 0; }
    sr_vk.vkFreeDescriptorSets = (PFN_vkFreeDescriptorSets)sr_vk.vkGetDeviceProcAddr(device, "vkFreeDescriptorSets");
    if (!sr_vk.vkFreeDescriptorSets) { if (ready) missing("device", "vkFreeDescriptorSets"); ready = 0; }
    sr_vk.vkFreeMemory = (PFN_vkFreeMemory)sr_vk.vkGetDeviceProcAddr(device, "vkFreeMemory");
    if (!sr_vk.vkFreeMemory) { if (ready) missing("device", "vkFreeMemory"); ready = 0; }
    sr_vk.vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)sr_vk.vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements");
    if (!sr_vk.vkGetBufferMemoryRequirements) { if (ready) missing("device", "vkGetBufferMemoryRequirements"); ready = 0; }
    sr_vk.vkGetDeviceQueue = (PFN_vkGetDeviceQueue)sr_vk.vkGetDeviceProcAddr(device, "vkGetDeviceQueue");
    if (!sr_vk.vkGetDeviceQueue) { if (ready) missing("device", "vkGetDeviceQueue"); ready = 0; }
    sr_vk.vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)sr_vk.vkGetDeviceProcAddr(device, "vkGetImageMemoryRequirements");
    if (!sr_vk.vkGetImageMemoryRequirements) { if (ready) missing("device", "vkGetImageMemoryRequirements"); ready = 0; }
    sr_vk.vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)sr_vk.vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR");
    if (!sr_vk.vkGetSwapchainImagesKHR) { if (ready) missing("device", "vkGetSwapchainImagesKHR"); ready = 0; }
    sr_vk.vkMapMemory = (PFN_vkMapMemory)sr_vk.vkGetDeviceProcAddr(device, "vkMapMemory");
    if (!sr_vk.vkMapMemory) { if (ready) missing("device", "vkMapMemory"); ready = 0; }
    sr_vk.vkQueuePresentKHR = (PFN_vkQueuePresentKHR)sr_vk.vkGetDeviceProcAddr(device, "vkQueuePresentKHR");
    if (!sr_vk.vkQueuePresentKHR) { if (ready) missing("device", "vkQueuePresentKHR"); ready = 0; }
    sr_vk.vkQueueSubmit = (PFN_vkQueueSubmit)sr_vk.vkGetDeviceProcAddr(device, "vkQueueSubmit");
    if (!sr_vk.vkQueueSubmit) { if (ready) missing("device", "vkQueueSubmit"); ready = 0; }
    sr_vk.vkQueueWaitIdle = (PFN_vkQueueWaitIdle)sr_vk.vkGetDeviceProcAddr(device, "vkQueueWaitIdle");
    if (!sr_vk.vkQueueWaitIdle) { if (ready) missing("device", "vkQueueWaitIdle"); ready = 0; }
    sr_vk.vkResetCommandBuffer = (PFN_vkResetCommandBuffer)sr_vk.vkGetDeviceProcAddr(device, "vkResetCommandBuffer");
    if (!sr_vk.vkResetCommandBuffer) { if (ready) missing("device", "vkResetCommandBuffer"); ready = 0; }
    sr_vk.vkResetFences = (PFN_vkResetFences)sr_vk.vkGetDeviceProcAddr(device, "vkResetFences");
    if (!sr_vk.vkResetFences) { if (ready) missing("device", "vkResetFences"); ready = 0; }
    sr_vk.vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)sr_vk.vkGetDeviceProcAddr(device, "vkUpdateDescriptorSets");
    if (!sr_vk.vkUpdateDescriptorSets) { if (ready) missing("device", "vkUpdateDescriptorSets"); ready = 0; }
    sr_vk.vkWaitForFences = (PFN_vkWaitForFences)sr_vk.vkGetDeviceProcAddr(device, "vkWaitForFences");
    if (!sr_vk.vkWaitForFences) { if (ready) missing("device", "vkWaitForFences"); ready = 0; }
    return ready;
}
