/* R32G32B32 uniform texel buffers (KosmicKrisp 0048).
 *
 * Metal has no 96-bit pixel format, so kk_get_va_format has no entry for R32G32B32 and kk_CreateBufferView
 * dereferenced its NULL result: vkd3d-proton creates such views for typed buffer SRVs of R32G32B32 DXGI
 * formats without checking the format's features (Halo Infinite), and the VMM died with SIGSEGV at address
 * 0xa in kk_CreateBufferView. The views are now R32 texture buffers three times as wide that shaders read
 * component by component. The compute shader reads views of each R32G32B32 format at offsets that are not
 * a multiple of 16 or of 12 (single texel alignment), and an R32_SFLOAT view as the unchanged path, and
 * checks every texel (alpha 1), textureSize and the buffer format features. A view of a format KosmicKrisp
 * has no texel buffer support for must fail instead of crashing.
 * Prints "RESULT: PASS|FAIL". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include "rgb32-texel-buffer-spv.h"

#define CHECK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { printf("RESULT: FAIL %s = %d\n", #x, r_); exit(1); } } while (0)

#define N 8
struct out { float f[N][4]; uint32_t u[N][4]; int32_t i[N][4]; float r[N][4]; int32_t sizes[4]; };

static VkDevice dev;
static VkPhysicalDeviceMemoryProperties mp;

static VkBuffer make_buffer(VkDeviceSize size, VkBufferUsageFlags usage, void **map)
{
   VkBufferCreateInfo bci = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = size, .usage = usage};
   VkBuffer buf; CHECK(vkCreateBuffer(dev, &bci, NULL, &buf));
   VkMemoryRequirements mr; vkGetBufferMemoryRequirements(dev, buf, &mr);
   uint32_t type = 0;
   const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
   while (type < mp.memoryTypeCount && (!(mr.memoryTypeBits & (1u << type)) || (mp.memoryTypes[type].propertyFlags & want) != want))
      type++;
   if (type == mp.memoryTypeCount) { printf("RESULT: FAIL no host-visible coherent memory type\n"); exit(1); }
   VkMemoryAllocateInfo mai = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, .allocationSize = mr.size, .memoryTypeIndex = type};
   VkDeviceMemory mem; CHECK(vkAllocateMemory(dev, &mai, NULL, &mem));
   CHECK(vkBindBufferMemory(dev, buf, mem, 0));
   CHECK(vkMapMemory(dev, mem, 0, VK_WHOLE_SIZE, 0, map));
   return buf;
}

static VkBufferView make_view(VkBuffer buf, VkFormat format, VkDeviceSize offset, VkDeviceSize range)
{
   VkBufferViewCreateInfo vci = {.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, .buffer = buf, .format = format, .offset = offset, .range = range};
   VkBufferView view; CHECK(vkCreateBufferView(dev, &vci, NULL, &view));
   return view;
}

int main(void)
{
   VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_3};
   VkInstanceCreateInfo ici = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app};
   VkInstance inst; CHECK(vkCreateInstance(&ici, NULL, &inst));
   uint32_t n = 1; VkPhysicalDevice pd; VkResult r = vkEnumeratePhysicalDevices(inst, &n, &pd);
   if ((r != VK_SUCCESS && r != VK_INCOMPLETE) || n < 1) { printf("RESULT: FAIL no physical device\n"); return 1; }
   vkGetPhysicalDeviceMemoryProperties(pd, &mp);

   const VkFormat rgb32[3] = {VK_FORMAT_R32G32B32_SFLOAT, VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SINT};
   for (unsigned k = 0; k < 3; k++) {
      VkFormatProperties fp; vkGetPhysicalDeviceFormatProperties(pd, rgb32[k], &fp);
      if (!(fp.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT) ||
          (fp.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT)) {
         printf("RESULT: FAIL format %d buffer features %#x (want uniform, not storage texel buffer)\n", rgb32[k], fp.bufferFeatures);
         return 1;
      }
   }

   float prio = 1;
   VkDeviceQueueCreateInfo qci = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueCount = 1, .pQueuePriorities = &prio};
   /* Vulkan 1.3: texel buffer offset alignment per the device's (single texel) alignment properties */
   VkDeviceCreateInfo dci = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci};
   CHECK(vkCreateDevice(pd, &dci, NULL, &dev));
   VkQueue q; vkGetDeviceQueue(dev, 0, 0, &q);

   /* One source buffer: word w holds w + 0.5 (floats), 1000 + w (uints) and -1000 - w (ints) in three
    * 4 KiB parts; the views start at words 5, 2 and 7 of their part (offsets 20, 8 and 28 bytes) and the
    * R32_SFLOAT view at word 3. */
   const uint32_t part = 1024, f_word = 5, u_word = 2, i_word = 7, r_word = 3;
   void *src_map;
   VkBuffer src = make_buffer(3 * part * 4, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, &src_map);
   float *fw = src_map; uint32_t *uw = (uint32_t *)src_map + part; int32_t *iw = (int32_t *)src_map + 2 * part;
   for (uint32_t w = 0; w < part; w++) { fw[w] = w + 0.5f; uw[w] = 1000 + w; iw[w] = -1000 - (int32_t)w; }
   void *out_map;
   VkBuffer out = make_buffer(sizeof(struct out), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &out_map);
   memset(out_map, 0xcd, sizeof(struct out));

   /* A texel buffer view of a format without texel buffer support is invalid usage, but a guest can send
    * one through Venus: it must fail, not take the host process down. */
   VkBufferViewCreateInfo bad = {.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, .buffer = src,
      .format = VK_FORMAT_R16G16B16_SFLOAT, .offset = 0, .range = 6 * N};
   VkBufferView bad_view = VK_NULL_HANDLE;
   if (vkCreateBufferView(dev, &bad, NULL, &bad_view) == VK_SUCCESS) {
      VkFormatProperties fp; vkGetPhysicalDeviceFormatProperties(pd, bad.format, &fp);
      if (!(fp.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT)) {
         printf("RESULT: FAIL R16G16B16_SFLOAT view created although the format has no texel buffer feature\n");
         return 1;
      }
      vkDestroyBufferView(dev, bad_view, NULL);
   }

   VkBufferView views[4] = {
      make_view(src, VK_FORMAT_R32G32B32_SFLOAT, f_word * 4, 12 * N),
      make_view(src, VK_FORMAT_R32G32B32_UINT, (part + u_word) * 4, 12 * N),
      make_view(src, VK_FORMAT_R32G32B32_SINT, (2 * part + i_word) * 4, 12 * N),
      make_view(src, VK_FORMAT_R32_SFLOAT, r_word * 4, 4 * N),
   };

   VkDescriptorSetLayoutBinding b[5];
   for (unsigned k = 0; k < 5; k++)
      b[k] = (VkDescriptorSetLayoutBinding){.binding = k, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
         .descriptorType = k < 4 ? VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};
   VkDescriptorSetLayoutCreateInfo dslci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 5, .pBindings = b};
   VkDescriptorSetLayout dsl; CHECK(vkCreateDescriptorSetLayout(dev, &dslci, NULL, &dsl));
   VkDescriptorPoolSize ps[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 4}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
   VkDescriptorPoolCreateInfo dpci = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1, .poolSizeCount = 2, .pPoolSizes = ps};
   VkDescriptorPool pool; CHECK(vkCreateDescriptorPool(dev, &dpci, NULL, &pool));
   VkDescriptorSetAllocateInfo dsai = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = pool, .descriptorSetCount = 1, .pSetLayouts = &dsl};
   VkDescriptorSet set; CHECK(vkAllocateDescriptorSets(dev, &dsai, &set));
   VkDescriptorBufferInfo obi = {.buffer = out, .offset = 0, .range = VK_WHOLE_SIZE};
   VkWriteDescriptorSet wr[5];
   for (unsigned k = 0; k < 5; k++)
      wr[k] = (VkWriteDescriptorSet){.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .dstBinding = k, .descriptorCount = 1,
         .descriptorType = b[k].descriptorType, .pTexelBufferView = k < 4 ? &views[k] : NULL, .pBufferInfo = k < 4 ? NULL : &obi};
   vkUpdateDescriptorSets(dev, 5, wr, 0, NULL);

   VkShaderModuleCreateInfo smci = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = sizeof(cs_spv), .pCode = (const uint32_t *)cs_spv};
   VkShaderModule cs; CHECK(vkCreateShaderModule(dev, &smci, NULL, &cs));
   VkPipelineLayoutCreateInfo plci = {.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .setLayoutCount = 1, .pSetLayouts = &dsl};
   VkPipelineLayout layout; CHECK(vkCreatePipelineLayout(dev, &plci, NULL, &layout));
   VkComputePipelineCreateInfo cpci = {.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .layout = layout,
      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = cs, .pName = "main"}};
   VkPipeline pipe; CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, NULL, &pipe));

   VkCommandPoolCreateInfo cpi = {.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
   VkCommandPool cmd_pool; CHECK(vkCreateCommandPool(dev, &cpi, NULL, &cmd_pool));
   VkCommandBufferAllocateInfo cbai = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = cmd_pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 1};
   VkCommandBuffer cmd; CHECK(vkAllocateCommandBuffers(dev, &cbai, &cmd));
   VkCommandBufferBeginInfo cbbi = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
   CHECK(vkBeginCommandBuffer(cmd, &cbbi));
   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, NULL);
   vkCmdDispatch(cmd, 1, 1, 1);
   VkMemoryBarrier mb = {.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &mb, 0, NULL, 0, NULL);
   CHECK(vkEndCommandBuffer(cmd));
   VkSubmitInfo si = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};
   CHECK(vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE));
   CHECK(vkQueueWaitIdle(q));

   const struct out *o = out_map;
   unsigned bad_texels = 0;
   for (unsigned x = 0; x < N; x++) {
      for (unsigned c = 0; c < 4; c++) {
         const float fe = c < 3 ? fw[f_word + 3 * x + c] : 1.0f;
         const uint32_t ue = c < 3 ? uw[u_word + 3 * x + c] : 1u;
         const int32_t ie = c < 3 ? iw[i_word + 3 * x + c] : 1;
         const float re = c == 0 ? fw[r_word + x] : c == 3 ? 1.0f : 0.0f;
         if (o->f[x][c] != fe || o->u[x][c] != ue || o->i[x][c] != ie || o->r[x][c] != re) {
            if (bad_texels++ < 8)
               printf("texel %u.%u: sfloat %g (want %g) uint %u (want %u) sint %d (want %d) r32 %g (want %g)\n", x, c,
                      o->f[x][c], fe, o->u[x][c], ue, o->i[x][c], ie, o->r[x][c], re);
         }
      }
   }
   const int size_ok = o->sizes[0] == N && o->sizes[1] == N && o->sizes[2] == N && o->sizes[3] == N;
   if (!size_ok)
      printf("textureSize: %d %d %d %d (want %d)\n", o->sizes[0], o->sizes[1], o->sizes[2], o->sizes[3], N);

   vkDestroyCommandPool(dev, cmd_pool, NULL);
   vkDestroyPipeline(dev, pipe, NULL);
   vkDestroyPipelineLayout(dev, layout, NULL);
   vkDestroyShaderModule(dev, cs, NULL);
   vkDestroyDescriptorPool(dev, pool, NULL);
   vkDestroyDescriptorSetLayout(dev, dsl, NULL);
   for (unsigned k = 0; k < 4; k++)
      vkDestroyBufferView(dev, views[k], NULL);
   vkDestroyDevice(dev, NULL);
   vkDestroyInstance(inst, NULL);
   if (bad_texels || !size_ok) {
      printf("RESULT: FAIL %u wrong texel components\n", bad_texels);
      return 1;
   }
   printf("RESULT: PASS R32G32B32 SFLOAT/UINT/SINT and R32_SFLOAT texel buffer fetches and sizes\n");
   return 0;
}
