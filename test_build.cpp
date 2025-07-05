#include <iostream>
#include "src/graphics/graphics_factory.h"

int main() {
    std::cout << "Testing Overlay Hook Project Structure..." << std::endl;
    
    // Test graphics factory
    auto api = GraphicsFactory::detectGraphicsAPI();
    
    switch (api) {
    case GraphicsAPI::DirectX9:
        std::cout << "Detected DirectX 9" << std::endl;
        break;
    case GraphicsAPI::DirectX10:
        std::cout << "Detected DirectX 10" << std::endl;
        break;
    case GraphicsAPI::DirectX11:
        std::cout << "Detected DirectX 11" << std::endl;
        break;
    case GraphicsAPI::DirectX12:
        std::cout << "Detected DirectX 12" << std::endl;
        break;
    default:
        std::cout << "No DirectX API detected" << std::endl;
        break;
    }
    
    std::cout << "Project structure test complete!" << std::endl;
    return 0;
} 