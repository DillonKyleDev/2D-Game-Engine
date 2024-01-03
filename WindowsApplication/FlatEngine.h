#pragma once
#include <stdio.h>
#include <string>
#include <sstream>
#include "SDL.h"
#include <SDL_syswm.h>
#include <imgui.h>
#include "Window.h"
#include "GameObject.h"
#include "SceneManager.h"
#include "Logger.h"

//ImGui - SDL Renderer
#pragma once
#include "imgui.h"
#include "../WindowsApplication/backends/imgui_impl_sdl2.h"
#include "../WindowsApplication/backends/imgui_impl_sdlrenderer2.h"


namespace FlatEngine
{
	// Variables
	extern int FocusedGameObjectIndex;
	void SetFocusedGameObjectIndex(int index);
	int GetFocusedGameObjectIndex();
	extern FlatEngine::SceneManager *sceneManager;
	extern FlatEngine::Logger* logger;

	namespace FlatGui
	{
		// Variables
		extern bool _editingValue;
		extern float childPadding;
		extern ImVec4 outerWindowColor;
		extern ImVec4 innerWindowColor;
		extern ImVec4 singleItemColor;
		extern int zoomLevels[];
		extern float zoomMultipliers[];
		extern int currentZoomLevel;
		extern float currentZoomMultiplier;
		extern float SCENE_VIEWPORT_WIDTH;
		extern float SCENE_VIEWPORT_HEIGHT;
		extern bool _viewportsHaveBeenSet;

		extern void SetupImGui();
		extern void Render(bool& quit);
		extern void AddViewports();
		extern void RenderHierarchy();
		extern void RenderInspector();
		extern void RenderSceneView();
		extern void IncreaseSceneZoom();
		extern void DecreaseSceneZoom();
		extern void RenderLog();
		extern void Cleanup();

		// Helper
		extern void AddImageToDrawList(SDL_Texture* texture, Vector2 position, float textureWidth, float textureHeight, Vector2 pivotPoint, ImVec2 scrolling, ImVec2 canvas_p0, Vector2 scale = { 1, 1 }, bool _scalesWithZoom = true);
	}
};



//DirectX12 stuff
//#include "imgui_impl_dx12.h"
//#include <d3d12.h>
//#include <dxgi1_4.h>
//
//#ifdef _DEBUG
//#define DX12_ENABLE_DEBUG_LAYER
//#endif
//
//#ifdef DX12_ENABLE_DEBUG_LAYER
//#include <dxgidebug.h>
//#pragma comment(lib, "dxguid.lib")
//#endif