// Runs after the unmodified class declarations and exact swapchain methods extracted by the runner.
int main() {
 using namespace plume;using namespace fixture;
 try {
 SDL_Window window;VulkanInterface iface;VulkanDevice device;device.renderInterface=&iface;
 std::mutex mutex;VulkanQueue queue{&mutex};VulkanCommandQueue commands{&device,&queue,0,{}};
 RenderSwapChainDesc desc;desc.renderWindow=&window;
 {
  VulkanSwapChain chain(&commands,desc);
  check(chain.getFormat()==RenderFormat::R8G8B8A8_UNORM,"constructor must choose advertised RGBA");
  check(driver.creates==0,"constructor should not use invalid deferred state");
  check(chain.resize() && !chain.isEmpty(),"valid RGBA surface did not create");
  check(driver.last.imageFormat==VK_FORMAT_R8G8B8A8_UNORM,"driver must receive RGBA");
  check(driver.last.minImageCount==3,"unbounded image count was clamped incorrectly");
  check(driver.last.imageUsage==VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,"unsupported sampled usage was requested");
  check(chain.getTextureCount()==3 && chain.getTexture(2)!=nullptr,"real images not populated");
  check(chain.getTexture(3)==nullptr,"out-of-range access not guarded");
  VulkanCommandSemaphore semaphore;uint32_t index=0;
  check(chain.acquireTexture(&semaphore,&index),"image acquisition failed");
  check(chain.present(index,nullptr,0),"presentation failed");
  check(!chain.present(3,nullptr,0) && driver.presents==1,"out-of-range presentation reached driver");
  driver.createResult=VK_ERROR_SURFACE_LOST_KHR;
  check(!chain.resize() && chain.isEmpty(),"failed resize must invalidate retired swapchain");
  check(driver.destroys==1 && driver.live.empty(),"retired swapchain was not disposed");
  check(chain.getTexture(0)==nullptr,"failure retained stale image access");
  check(!chain.acquireTexture(&semaphore,&index) && driver.acquires==1,"null acquire reached driver");
  check(!chain.present(0,nullptr,0) && driver.presents==1,"null present reached driver");
  check(chain.getRefreshRate()==0,"timing query used invalid handle");
 }
 check(driver.badHandles==0 && driver.live.empty(),"invalid Vulkan handle or double destroy");
 check(commands.swapChains.empty(),"destructor did not remove queue tracking");
 driver=Driver{};driver.formats={{VK_FORMAT_R16G16B16A16_SFLOAT,VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
 {
  VulkanSwapChain chain(&commands,desc);
  check(!chain.resize() && chain.isEmpty(),"unsupported format constructor must fail closed");
  check(driver.creates==0,"unsupported format must never reach vkCreateSwapchainKHR");
 }
 driver=Driver{};driver.queryResult=VK_ERROR_SURFACE_LOST_KHR;
 {VulkanSwapChain chain(&commands,desc);check(!chain.resize() && driver.creates==0,"failed query must fail before driver creation");}
 driver=Driver{};driver.failView=true;
 {VulkanSwapChain chain(&commands,desc);check(!chain.resize() && chain.isEmpty() && chain.getTexture(0)==nullptr,"image-view failure must discard incomplete images");}
 check(driver.badHandles==0 && driver.live.empty(),"image-view failure leaked/double-destroyed swapchain");
 driver=Driver{};window.width=0;
 {VulkanSwapChain chain(&commands,desc);check(!chain.resize() && driver.creates==0,"zero-sized window attempted creation");}
 window.width=2340;driver=Driver{};driver.caps.supportedUsageFlags=0;
 {VulkanSwapChain chain(&commands,desc);check(!chain.resize() && driver.creates==0,"unsupported color output attempted creation");}
 std::cout<<"PASS: "<<checks<<" actual Plume swapchain method checks with mocked SDL/Vulkan (not gameplay)\n";
 }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
