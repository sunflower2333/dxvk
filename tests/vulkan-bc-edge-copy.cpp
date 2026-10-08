#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// Standalone public Vulkan composition probe. Run with a CPU ICD; this does
// not instantiate DXVK, the UMD, or a hardware driver admission path.
static unsigned checks;
static const char* phase = "start";
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"Vulkan BC edge failure phase=%s line=%d: %s\n",phase,__LINE__,#x); std::exit(1); } } while (0)
#define VKCHECK(x) do { VkResult r=(x); ++checks; if (r!=VK_SUCCESS) { std::fprintf(stderr,"Vulkan BC edge API failure phase=%s line=%d result=%d: %s\n",phase,__LINE__,int(r),#x); std::exit(1); } } while (0)
struct Case { unsigned sm,dm,sx,sy,dx,dy,w,h,layers; };
static constexpr Case CASES[] = {
  {3,0,0,0,4,12,4,4,1}, {2,0,4,0,8,4,4,4,1},
  {0,2,0,0,4,0,4,4,1}, {0,3,0,0,0,0,4,4,1},
  {3,4,0,0,0,0,4,4,1}, {2,2,4,0,4,0,4,4,1},
  {0,0,4,4,8,4,4,4,1}, {1,2,4,0,0,0,8,4,2},
};
static unsigned width(unsigned mip) { return std::max(1u,24u>>mip); }
static unsigned height(unsigned mip) { return std::max(1u,16u>>mip); }
static size_t mipBytes(unsigned mip,unsigned bytes) { return size_t((width(mip)+3)/4)*((height(mip)+3)/4)*bytes; }
static size_t layerBytes(unsigned bytes) { size_t n=0; for(unsigned m=0;m<5;++m)n+=mipBytes(m,bytes); return n; }
static size_t offset(unsigned mip,unsigned layer,unsigned bytes) {
  size_t n=layer*layerBytes(bytes); for(unsigned m=0;m<mip;++m)n+=mipBytes(m,bytes); return n;
}
static std::vector<unsigned char> pattern(unsigned seed,unsigned bytes) {
  std::vector<unsigned char> data(2*layerBytes(bytes));
  for(unsigned l=0;l<2;++l)for(unsigned m=0;m<5;++m)
    for(size_t i=0;i<mipBytes(m,bytes);++i)data[offset(m,l,bytes)+i]=static_cast<unsigned char>(seed+17*l+29*m+7*i+(i>>3));
  return data;
}
static unsigned memoryType(VkPhysicalDevice physical,unsigned mask,VkMemoryPropertyFlags flags) {
  VkPhysicalDeviceMemoryProperties p{}; vkGetPhysicalDeviceMemoryProperties(physical,&p);
  for(unsigned i=0;i<p.memoryTypeCount;++i)if((mask&(1u<<i))&&(p.memoryTypes[i].propertyFlags&flags)==flags)return i;
  CHECK(false); return 0;
}
struct Buffer { VkBuffer buffer{}; VkDeviceMemory memory{}; };
static Buffer buffer(VkDevice device,VkPhysicalDevice physical,size_t bytes,VkBufferUsageFlags usage,VkMemoryPropertyFlags flags) {
  Buffer b; VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; ci.size=bytes; ci.usage=usage; ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  VKCHECK(vkCreateBuffer(device,&ci,nullptr,&b.buffer)); VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(device,b.buffer,&mr);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.allocationSize=mr.size; ai.memoryTypeIndex=memoryType(physical,mr.memoryTypeBits,flags);
  VKCHECK(vkAllocateMemory(device,&ai,nullptr,&b.memory)); VKCHECK(vkBindBufferMemory(device,b.buffer,b.memory,0)); return b;
}
struct Image { VkImage image{}; VkDeviceMemory memory{}; };
static Image image(VkDevice device,VkPhysicalDevice physical,VkFormat format) {
  VkFormatProperties props{}; vkGetPhysicalDeviceFormatProperties(physical,format,&props);
  CHECK((props.optimalTilingFeatures&(VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT))==(VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT));
  Image im; VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; ci.imageType=VK_IMAGE_TYPE_2D; ci.format=format; ci.extent={24,16,1};
  ci.mipLevels=5; ci.arrayLayers=2; ci.samples=VK_SAMPLE_COUNT_1_BIT; ci.tiling=VK_IMAGE_TILING_OPTIMAL;
  ci.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT; ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  VKCHECK(vkCreateImage(device,&ci,nullptr,&im.image)); VkMemoryRequirements mr{}; vkGetImageMemoryRequirements(device,im.image,&mr);
  VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.allocationSize=mr.size; ai.memoryTypeIndex=memoryType(physical,mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VKCHECK(vkAllocateMemory(device,&ai,nullptr,&im.memory)); VKCHECK(vkBindImageMemory(device,im.image,im.memory,0)); return im;
}
static void barrier(VkCommandBuffer cmd,VkAccessFlags source,VkAccessFlags destination,VkPipelineStageFlags destinationStage=VK_PIPELINE_STAGE_TRANSFER_BIT) {
  VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER}; b.srcAccessMask=source; b.dstAccessMask=destination;
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,destinationStage,0,1,&b,0,nullptr,0,nullptr);
}
static std::vector<VkBufferImageCopy> fullRegions(size_t base,unsigned bytes) {
  std::vector<VkBufferImageCopy> result;
  for(unsigned l=0;l<2;++l)for(unsigned m=0;m<5;++m) {
    VkBufferImageCopy r{}; r.bufferOffset=base+offset(m,l,bytes); r.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,m,l,1}; r.imageExtent={width(m),height(m),1}; result.push_back(r);
  }
  return result;
}
static void save(unsigned bytes,unsigned index,const char* role,const void* data,size_t size) {
  char name[128]; std::snprintf(name,sizeof(name),"vulkan-bc-%u-%u-%s.bin",bytes,index,role);
  FILE* out=std::fopen(name,"wb"); CHECK(out); CHECK(std::fwrite(data,1,size,out)==size); CHECK(std::fclose(out)==0);
}
int main(int argc,char** argv) {
  CHECK(argc==3); const unsigned bytes=unsigned(std::strtoul(argv[1],nullptr,10)),index=unsigned(std::strtoul(argv[2],nullptr,10));
  CHECK((bytes==8||bytes==16)&&index<8); const auto c=CASES[index];
  const auto source=pattern(0x31,bytes),destination=pattern(0xa3,bytes); auto expected=destination;
  const unsigned firstLayer=c.layers==2?0u:1u;
  for(unsigned l=firstLayer;l<firstLayer+c.layers;++l)for(unsigned row=0;row<c.h/4;++row) {
    const size_t s=offset(c.sm,l,bytes)+size_t(c.sy/4+row)*((width(c.sm)+3)/4)*bytes+c.sx/4*bytes;
    const size_t d=offset(c.dm,l,bytes)+size_t(c.dy/4+row)*((width(c.dm)+3)/4)*bytes+c.dx/4*bytes;
    std::memcpy(expected.data()+d,source.data()+s,c.w/4*bytes);
  }
  save(bytes,index,"source-initial",source.data(),source.size()); save(bytes,index,"destination-initial",destination.data(),destination.size());
  save(bytes,index,"destination-expected",expected.data(),expected.size());
  phase="create-instance"; VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO}; app.apiVersion=VK_API_VERSION_1_1;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo=&app; VkInstance instance{};
  VKCHECK(vkCreateInstance(&ici,nullptr,&instance)); unsigned count=0; VKCHECK(vkEnumeratePhysicalDevices(instance,&count,nullptr)); CHECK(count>0);
  std::vector<VkPhysicalDevice> physicals(count); VKCHECK(vkEnumeratePhysicalDevices(instance,&count,physicals.data())); VkPhysicalDevice physical{};
  VkPhysicalDeviceProperties properties{};
  for(auto p:physicals) { VkPhysicalDeviceProperties candidate{}; vkGetPhysicalDeviceProperties(p,&candidate); if(candidate.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU){physical=p;properties=candidate;break;} }
  CHECK(physical); std::printf("Vulkan BC CPU reference device=%s api=%u hardware_admission=0\n",properties.deviceName,properties.apiVersion); std::fflush(stdout);
  unsigned queues=0; vkGetPhysicalDeviceQueueFamilyProperties(physical,&queues,nullptr); std::vector<VkQueueFamilyProperties> qp(queues); vkGetPhysicalDeviceQueueFamilyProperties(physical,&queues,qp.data());
  unsigned family=queues; for(unsigned i=0;i<queues;++i)if(qp[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){family=i;break;} CHECK(family<queues);
  VkPhysicalDeviceFeatures available{}; vkGetPhysicalDeviceFeatures(physical,&available); CHECK(available.textureCompressionBC);
  VkPhysicalDeviceFeatures features{}; features.textureCompressionBC=VK_TRUE; float priority=1;
  VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qi.queueFamilyIndex=family; qi.queueCount=1; qi.pQueuePriorities=&priority;
  VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.queueCreateInfoCount=1; di.pQueueCreateInfos=&qi; di.pEnabledFeatures=&features;
  phase="create-device"; VkDevice device{}; VKCHECK(vkCreateDevice(physical,&di,nullptr,&device)); VkQueue queue{}; vkGetDeviceQueue(device,family,0,&queue);
  const VkFormat sf=bytes==8?VK_FORMAT_BC1_RGBA_UNORM_BLOCK:VK_FORMAT_BC3_UNORM_BLOCK,df=bytes==8?VK_FORMAT_BC1_RGBA_SRGB_BLOCK:VK_FORMAT_BC3_SRGB_BLOCK;
  auto src=image(device,physical,sf),dst=image(device,physical,df);
  const auto host=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  auto upload=buffer(device,physical,source.size()*2,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,host);
  auto readback=buffer(device,physical,source.size()*2+32,VK_BUFFER_USAGE_TRANSFER_DST_BIT,host);
  const size_t encoded=bytes*(c.w/4)*(c.h/4)*c.layers;
  auto transfer=buffer(device,physical,encoded,VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  void* mapped{}; VKCHECK(vkMapMemory(device,upload.memory,0,VK_WHOLE_SIZE,0,&mapped)); std::memcpy(mapped,source.data(),source.size()); std::memcpy(static_cast<unsigned char*>(mapped)+source.size(),destination.data(),destination.size()); vkUnmapMemory(device,upload.memory);
  VKCHECK(vkMapMemory(device,readback.memory,0,VK_WHOLE_SIZE,0,&mapped)); std::memset(mapped,0xe7,source.size()*2+32); vkUnmapMemory(device,readback.memory);
  VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pci.queueFamilyIndex=family; VkCommandPool pool{}; VKCHECK(vkCreateCommandPool(device,&pci,nullptr,&pool));
  VkCommandBufferAllocateInfo cai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; cai.commandPool=pool; cai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount=1;
  VkCommandBuffer cmd{}; VKCHECK(vkAllocateCommandBuffers(device,&cai,&cmd)); VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; VKCHECK(vkBeginCommandBuffer(cmd,&bi));
  VkImageMemoryBarrier ib[2]{};
  for(unsigned i=0;i<2;++i) { ib[i].sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; ib[i].oldLayout=VK_IMAGE_LAYOUT_UNDEFINED; ib[i].newLayout=VK_IMAGE_LAYOUT_GENERAL; ib[i].dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_TRANSFER_READ_BIT; ib[i].srcQueueFamilyIndex=ib[i].dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; ib[i].image=i?dst.image:src.image; ib[i].subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,5,0,2}; }
  vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,2,ib);
  auto sr=fullRegions(0,bytes),dr=fullRegions(source.size(),bytes);
  vkCmdCopyBufferToImage(cmd,upload.buffer,src.image,VK_IMAGE_LAYOUT_GENERAL,unsigned(sr.size()),sr.data());
  vkCmdCopyBufferToImage(cmd,upload.buffer,dst.image,VK_IMAGE_LAYOUT_GENERAL,unsigned(dr.size()),dr.data());
  barrier(cmd,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT);
  const VkExtent3D se{std::min(c.w,width(c.sm)-c.sx),std::min(c.h,height(c.sm)-c.sy),1};
  const VkExtent3D de{std::min(c.w,width(c.dm)-c.dx),std::min(c.h,height(c.dm)-c.dy),1};
  CHECK((se.width+3)/4==(de.width+3)/4&&(se.height+3)/4==(de.height+3)/4);
  CHECK((se.width%4==0||c.sx+se.width==width(c.sm))&&(se.height%4==0||c.sy+se.height==height(c.sm)));
  CHECK((de.width%4==0||c.dx+de.width==width(c.dm))&&(de.height%4==0||c.dy+de.height==height(c.dm)));
  const bool bridge=index!=5&&index!=6; phase="copy";
  if(bridge) {
    VkBufferImageCopy from{}; from.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,c.sm,firstLayer,c.layers}; from.imageOffset={int(c.sx),int(c.sy),0}; from.imageExtent=se;
    vkCmdCopyImageToBuffer(cmd,src.image,VK_IMAGE_LAYOUT_GENERAL,transfer.buffer,1,&from);
    barrier(cmd,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy to{}; to.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,c.dm,firstLayer,c.layers}; to.imageOffset={int(c.dx),int(c.dy),0}; to.imageExtent=de;
    vkCmdCopyBufferToImage(cmd,transfer.buffer,dst.image,VK_IMAGE_LAYOUT_GENERAL,1,&to);
  } else {
    CHECK(se.width==de.width&&se.height==de.height); VkImageCopy copy{};
    copy.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,c.sm,firstLayer,c.layers}; copy.srcOffset={int(c.sx),int(c.sy),0};
    copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,c.dm,firstLayer,c.layers}; copy.dstOffset={int(c.dx),int(c.dy),0}; copy.extent=se;
    vkCmdCopyImage(cmd,src.image,VK_IMAGE_LAYOUT_GENERAL,dst.image,VK_IMAGE_LAYOUT_GENERAL,1,&copy);
  }
  barrier(cmd,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT); auto readSrc=fullRegions(16,bytes),readDst=fullRegions(16+source.size(),bytes);
  vkCmdCopyImageToBuffer(cmd,src.image,VK_IMAGE_LAYOUT_GENERAL,readback.buffer,unsigned(readSrc.size()),readSrc.data());
  vkCmdCopyImageToBuffer(cmd,dst.image,VK_IMAGE_LAYOUT_GENERAL,readback.buffer,unsigned(readDst.size()),readDst.data());
  barrier(cmd,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT,VK_PIPELINE_STAGE_HOST_BIT); VKCHECK(vkEndCommandBuffer(cmd));
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.commandBufferCount=1; submit.pCommandBuffers=&cmd;
  phase="submit"; VKCHECK(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE)); VKCHECK(vkQueueWaitIdle(queue));
  phase="literal-readback"; VKCHECK(vkMapMemory(device,readback.memory,0,VK_WHOLE_SIZE,0,&mapped)); auto* data=static_cast<unsigned char*>(mapped);
  save(bytes,index,"source-actual",data+16,source.size()); save(bytes,index,"destination-actual",data+16+source.size(),destination.size());
  save(bytes,index,"readback-with-guards",data,source.size()*2+32);
  CHECK(std::memcmp(data+16,source.data(),source.size())==0); CHECK(std::memcmp(data+16+source.size(),expected.data(),expected.size())==0);
  for(unsigned i=0;i<16;++i) { CHECK(data[i]==0xe7); CHECK(data[16+source.size()*2+i]==0xe7); }
  vkUnmapMemory(device,readback.memory); VKCHECK(vkDeviceWaitIdle(device));
  vkDestroyCommandPool(device,pool,nullptr);
  for(auto b:{upload,readback,transfer}) { vkDestroyBuffer(device,b.buffer,nullptr); vkFreeMemory(device,b.memory,nullptr); }
  for(auto im:{src,dst}) { vkDestroyImage(device,im.image,nullptr); vkFreeMemory(device,im.memory,nullptr); }
  vkDestroyDevice(device,nullptr); vkDestroyInstance(instance,nullptr);
  std::printf("Vulkan BC edge composition PASS checks=%u block_bytes=%u case=%u bridge=%u layers=%u source_bytes=%zu destination_bytes=%zu raw_files=6 hardware_admission=0\n",checks,bytes,index,unsigned(bridge),c.layers,source.size(),destination.size());
}
