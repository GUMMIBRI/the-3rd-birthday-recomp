/* SPDX-License-Identifier: GPL-2.0-or-later
 * Checked Vulkan dispatch for a statically provided ICD, no global loader exports.
 * Define SR_VK_DISPATCH before ANY Vulkan header in participating renderer TUs.
 * Without that define, existing Windows Vulkan prototypes/calls are unchanged.
 */
#ifndef SR_VK_DISPATCH_H
#define SR_VK_DISPATCH_H
#if defined(SR_VK_DISPATCH) && !defined(VK_NO_PROTOTYPES)
#if defined(VULKAN_H_) || defined(VULKAN_CORE_H_)
#error "Include vk_dispatch.h before Vulkan headers when using SR_VK_DISPATCH"
#endif
#define VK_NO_PROTOTYPES 1
#endif
#include <vulkan/vulkan.h>

/* One instance/device per renderer, initialized sequentially on its owner thread.
 * Global -> instance -> device loading is mandatory. A failed load exposes only
 * successfully found pointers for checked cleanup; do not render after failure.
 * Reset only AFTER Vulkan resources/instance are destroyed and BEFORE SDL unload.
 */
typedef struct SrVkDispatch {
    PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
    PFN_vkAllocateMemory vkAllocateMemory;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkBindBufferMemory vkBindBufferMemory;
    PFN_vkBindImageMemory vkBindImageMemory;
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    PFN_vkCmdBindPipeline vkCmdBindPipeline;
    PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
    PFN_vkCmdBlitImage vkCmdBlitImage;
    PFN_vkCmdClearColorImage vkCmdClearColorImage;
    PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
    PFN_vkCmdCopyImage vkCmdCopyImage;
    PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
    PFN_vkCmdDraw vkCmdDraw;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkCmdPushConstants vkCmdPushConstants;
    PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
    PFN_vkCmdSetScissor vkCmdSetScissor;
    PFN_vkCmdSetViewport vkCmdSetViewport;
    PFN_vkCreateBuffer vkCreateBuffer;
    PFN_vkCreateCommandPool vkCreateCommandPool;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
    PFN_vkCreateDevice vkCreateDevice;
    PFN_vkCreateFence vkCreateFence;
    PFN_vkCreateFramebuffer vkCreateFramebuffer;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
    PFN_vkCreateImage vkCreateImage;
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkCreateInstance vkCreateInstance;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    PFN_vkCreateRenderPass vkCreateRenderPass;
    PFN_vkCreateSampler vkCreateSampler;
    PFN_vkCreateSemaphore vkCreateSemaphore;
    PFN_vkCreateShaderModule vkCreateShaderModule;
    PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
    PFN_vkDestroyBuffer vkDestroyBuffer;
    PFN_vkDestroyCommandPool vkDestroyCommandPool;
    PFN_vkDestroyDevice vkDestroyDevice;
    PFN_vkDestroyFence vkDestroyFence;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
    PFN_vkDestroyImage vkDestroyImage;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkDestroyInstance vkDestroyInstance;
    PFN_vkDestroySampler vkDestroySampler;
    PFN_vkDestroySemaphore vkDestroySemaphore;
    PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
    PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
    PFN_vkFreeMemory vkFreeMemory;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
    PFN_vkGetDeviceQueue vkGetDeviceQueue;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
    PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
    PFN_vkMapMemory vkMapMemory;
    PFN_vkQueuePresentKHR vkQueuePresentKHR;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkQueueWaitIdle vkQueueWaitIdle;
    PFN_vkResetCommandBuffer vkResetCommandBuffer;
    PFN_vkResetFences vkResetFences;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    PFN_vkWaitForFences vkWaitForFences;
} SrVkDispatch;
extern SrVkDispatch sr_vk;
int sr_vk_load_global(void);
int sr_vk_load_instance(VkInstance instance);
int sr_vk_load_device(VkDevice device);
void sr_vk_reset(void);

#if defined(SR_VK_DISPATCH) && !defined(SR_VK_DISPATCH_IMPLEMENTATION)
#define vkAcquireNextImageKHR (sr_vk.vkAcquireNextImageKHR)
#define vkAllocateCommandBuffers (sr_vk.vkAllocateCommandBuffers)
#define vkAllocateDescriptorSets (sr_vk.vkAllocateDescriptorSets)
#define vkAllocateMemory (sr_vk.vkAllocateMemory)
#define vkBeginCommandBuffer (sr_vk.vkBeginCommandBuffer)
#define vkBindBufferMemory (sr_vk.vkBindBufferMemory)
#define vkBindImageMemory (sr_vk.vkBindImageMemory)
#define vkCmdBeginRenderPass (sr_vk.vkCmdBeginRenderPass)
#define vkCmdBindDescriptorSets (sr_vk.vkCmdBindDescriptorSets)
#define vkCmdBindPipeline (sr_vk.vkCmdBindPipeline)
#define vkCmdBindVertexBuffers (sr_vk.vkCmdBindVertexBuffers)
#define vkCmdBlitImage (sr_vk.vkCmdBlitImage)
#define vkCmdClearColorImage (sr_vk.vkCmdClearColorImage)
#define vkCmdClearDepthStencilImage (sr_vk.vkCmdClearDepthStencilImage)
#define vkCmdCopyBufferToImage (sr_vk.vkCmdCopyBufferToImage)
#define vkCmdCopyImage (sr_vk.vkCmdCopyImage)
#define vkCmdCopyImageToBuffer (sr_vk.vkCmdCopyImageToBuffer)
#define vkCmdDraw (sr_vk.vkCmdDraw)
#define vkCmdEndRenderPass (sr_vk.vkCmdEndRenderPass)
#define vkCmdPipelineBarrier (sr_vk.vkCmdPipelineBarrier)
#define vkCmdPushConstants (sr_vk.vkCmdPushConstants)
#define vkCmdSetBlendConstants (sr_vk.vkCmdSetBlendConstants)
#define vkCmdSetScissor (sr_vk.vkCmdSetScissor)
#define vkCmdSetViewport (sr_vk.vkCmdSetViewport)
#define vkCreateBuffer (sr_vk.vkCreateBuffer)
#define vkCreateCommandPool (sr_vk.vkCreateCommandPool)
#define vkCreateDescriptorPool (sr_vk.vkCreateDescriptorPool)
#define vkCreateDescriptorSetLayout (sr_vk.vkCreateDescriptorSetLayout)
#define vkCreateDevice (sr_vk.vkCreateDevice)
#define vkCreateFence (sr_vk.vkCreateFence)
#define vkCreateFramebuffer (sr_vk.vkCreateFramebuffer)
#define vkCreateGraphicsPipelines (sr_vk.vkCreateGraphicsPipelines)
#define vkCreateImage (sr_vk.vkCreateImage)
#define vkCreateImageView (sr_vk.vkCreateImageView)
#define vkCreateInstance (sr_vk.vkCreateInstance)
#define vkCreatePipelineLayout (sr_vk.vkCreatePipelineLayout)
#define vkCreateRenderPass (sr_vk.vkCreateRenderPass)
#define vkCreateSampler (sr_vk.vkCreateSampler)
#define vkCreateSemaphore (sr_vk.vkCreateSemaphore)
#define vkCreateShaderModule (sr_vk.vkCreateShaderModule)
#define vkCreateSwapchainKHR (sr_vk.vkCreateSwapchainKHR)
#define vkDestroyBuffer (sr_vk.vkDestroyBuffer)
#define vkDestroyCommandPool (sr_vk.vkDestroyCommandPool)
#define vkDestroyDevice (sr_vk.vkDestroyDevice)
#define vkDestroyFence (sr_vk.vkDestroyFence)
#define vkDestroyFramebuffer (sr_vk.vkDestroyFramebuffer)
#define vkDestroyImage (sr_vk.vkDestroyImage)
#define vkDestroyImageView (sr_vk.vkDestroyImageView)
#define vkDestroyInstance (sr_vk.vkDestroyInstance)
#define vkDestroySampler (sr_vk.vkDestroySampler)
#define vkDestroySemaphore (sr_vk.vkDestroySemaphore)
#define vkDestroySurfaceKHR (sr_vk.vkDestroySurfaceKHR)
#define vkDestroySwapchainKHR (sr_vk.vkDestroySwapchainKHR)
#define vkDeviceWaitIdle (sr_vk.vkDeviceWaitIdle)
#define vkEndCommandBuffer (sr_vk.vkEndCommandBuffer)
#define vkEnumeratePhysicalDevices (sr_vk.vkEnumeratePhysicalDevices)
#define vkFreeDescriptorSets (sr_vk.vkFreeDescriptorSets)
#define vkFreeMemory (sr_vk.vkFreeMemory)
#define vkGetBufferMemoryRequirements (sr_vk.vkGetBufferMemoryRequirements)
#define vkGetDeviceProcAddr (sr_vk.vkGetDeviceProcAddr)
#define vkGetDeviceQueue (sr_vk.vkGetDeviceQueue)
#define vkGetImageMemoryRequirements (sr_vk.vkGetImageMemoryRequirements)
#define vkGetPhysicalDeviceFeatures (sr_vk.vkGetPhysicalDeviceFeatures)
#define vkGetPhysicalDeviceMemoryProperties (sr_vk.vkGetPhysicalDeviceMemoryProperties)
#define vkGetPhysicalDeviceProperties (sr_vk.vkGetPhysicalDeviceProperties)
#define vkGetPhysicalDeviceQueueFamilyProperties (sr_vk.vkGetPhysicalDeviceQueueFamilyProperties)
#define vkGetPhysicalDeviceSurfaceCapabilitiesKHR (sr_vk.vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
#define vkGetPhysicalDeviceSurfaceFormatsKHR (sr_vk.vkGetPhysicalDeviceSurfaceFormatsKHR)
#define vkGetPhysicalDeviceSurfacePresentModesKHR (sr_vk.vkGetPhysicalDeviceSurfacePresentModesKHR)
#define vkGetPhysicalDeviceSurfaceSupportKHR (sr_vk.vkGetPhysicalDeviceSurfaceSupportKHR)
#define vkGetSwapchainImagesKHR (sr_vk.vkGetSwapchainImagesKHR)
#define vkMapMemory (sr_vk.vkMapMemory)
#define vkQueuePresentKHR (sr_vk.vkQueuePresentKHR)
#define vkQueueSubmit (sr_vk.vkQueueSubmit)
#define vkQueueWaitIdle (sr_vk.vkQueueWaitIdle)
#define vkResetCommandBuffer (sr_vk.vkResetCommandBuffer)
#define vkResetFences (sr_vk.vkResetFences)
#define vkUpdateDescriptorSets (sr_vk.vkUpdateDescriptorSets)
#define vkWaitForFences (sr_vk.vkWaitForFences)
#endif
#endif
