#include "FlatEngine.h"
#include "Transform.h"
#include "Camera.h"
#include "Scene.h"
#include "Sprite.h"
#include "Text.h"
#include "Animation.h"

namespace FlatEngine { namespace FlatGui {

	// Game view default values
	float GAME_VIEWPORT_WIDTH = 600;
	float GAME_VIEWPORT_HEIGHT = 400;
	float xGameCenter = 600 / 2;
	float yGameCenter = 400 / 2;


	void Game_RenderView()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);

		ImGuiWindowFlags flags = ImGuiWindowFlags_None;


		// If Release - Make GameView full screen and disable tab decoration and resizing
		if (FlatEngine::_isDebugMode == false)
		{
			// Get InputOutput
			ImGuiIO& inputOutput = ImGui::GetIO();
			float xSize = inputOutput.DisplaySize.x;
			float ySize = inputOutput.DisplaySize.y;
			ImGui::SetNextWindowSize(ImVec2(xSize, ySize));
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize;
		}

		bool _open = true;

		ImGui::Begin("Game View", &_open, flags);

		if (ImGui::IsWindowFocused())
			if (ImGui::IsKeyPressed(ImGuiKey_Escape))
				FlatEngine::gameManager->PauseGame();

		static bool opt_enable_context_menu = true;

		ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
		ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
		if (canvas_sz.x < 50.0f) canvas_sz.x = 50.0f;
		if (canvas_sz.y < 50.0f) canvas_sz.y = 50.0f;
		ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

		// Set viewport dimensions for rendering objects in game view. We want this to always be centered in game view so we can get it every frame.
		GAME_VIEWPORT_WIDTH = canvas_p1.x - canvas_p0.x + 1;
		GAME_VIEWPORT_HEIGHT = canvas_p1.y - canvas_p0.y + 1;

		// This will catch our interactions
		ImGui::InvisibleButton("GameViewCanvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		const bool is_hovered = ImGui::IsItemHovered(); // Hovered
		const bool is_active = ImGui::IsItemActive();   // Held

		// Render GameObjects in game view
		Game_RenderObjects(canvas_p0, canvas_sz);

		ImGui::End();

		// Reset WindowRounding
		ImGui::PopStyleVar();
		// Reset WindowPadding
		ImGui::PopStyleVar();
		// Reset WindowBorder
		ImGui::PopStyleVar();
	}

	void Game_RenderObjects(ImVec2 canvas_p0, ImVec2 canvas_sz)
	{
		// Get loaded scene if it's not a nullptr and initialize necessary entities
		std::shared_ptr<Scene> loadedScene = FlatEngine::GetLoadedScene();
		std::vector<std::shared_ptr<GameObject>> sceneObjects;
		std::shared_ptr<FlatEngine::Camera> primaryCamera;
		std::shared_ptr<FlatEngine::Transform> cameraTransform;

		if (loadedScene != nullptr)
		{
			sceneObjects = loadedScene->GetSceneObjects();
			// Get Primary Camera and set default values for if no camera is set
			primaryCamera = loadedScene->GetPrimaryCamera();
		}
		else
		{
			sceneObjects = std::vector <std::shared_ptr<GameObject>>();
			primaryCamera = nullptr;
		}

		// Create Draw List
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		// Create the splitter for the draw_list
		ImDrawListSplitter* drawSplitter = new ImDrawListSplitter();
		// 3 channels for now in this scene view. 0 = scene objects, 1 = other UI (camera icon, etc), 2 = transform arrow
		drawSplitter->Split(draw_list, maxSpriteLayers + 5);

		ImVec2 cameraPosition(0, 0);
		float cameraWidth = 50;
		float cameraHeight = 30;
		// Used to convert grid space values to pixel values.ie. 2 grid squares = 2 * 10 = 20px.
		float cameraZoom = 10;
		ImVec4 frustrumColor = ImVec4(1, 1, 1, 1);


		// If the primaryCamera is found and not nullptr, set the cameraPosition accordingly, else it remains at {0,0} above
		if (primaryCamera != nullptr)
		{
			cameraTransform = std::static_pointer_cast<FlatEngine::Transform>(FlatEngine::GetObjectComponent(primaryCamera->GetParentID(), ComponentTypes::Transform));
			cameraWidth = primaryCamera->GetWidth();
			cameraHeight = primaryCamera->GetHeight();
			cameraZoom = primaryCamera->GetZoom();
			frustrumColor = primaryCamera->GetFrustrumColor();

			// Get the cameras position including all of its parents transforms offsets using the recusive Game_GetTotalCameraOffset();
			cameraPosition = Game_GetTotalCameraOffset(primaryCamera);
		}

		// Get the "center point" of the games view. This will appear to move around when we move the camera
		worldCenterPoint = ImVec2((GAME_VIEWPORT_WIDTH / 2) - (cameraPosition.x * cameraZoom) + canvas_p0.x, (GAME_VIEWPORT_HEIGHT / 2) + (cameraPosition.y * cameraZoom) + canvas_p0.y);
		// Get the "center point of the viewport
		ImVec2 viewportCenterPoint = ImVec2((GAME_VIEWPORT_WIDTH / 2) + canvas_p0.x, (GAME_VIEWPORT_HEIGHT / 2) + canvas_p0.y);

		// Start off with a 0,0 parentOffset because this is the top level object to be rendered.
		Vector2 parentOffset(0, 0);
		Vector2 parentScale(1, 1);

		// Render Game Objects
		for (int i = 0; i < sceneObjects.size(); i++)
		{
			// If this object doesn't have a parent, render it and then its children recursively
			if (sceneObjects[i]->GetParentID() == -1 && sceneObjects[i]->IsActive())
				Game_RenderSelfThenChildren(sceneObjects[i], parentOffset, parentScale, worldCenterPoint, canvas_p0, canvas_sz, draw_list, drawSplitter,
					cameraPosition, cameraWidth, cameraHeight, cameraZoom);
		}


		// Render Primary Camera Frustrum
		{
			Texture* frustrum = new Texture();
			frustrum->loadFromFile("assets/images/frustrum.png");
			float h_FrustWidth = GAME_VIEWPORT_WIDTH;
			float h_FrustHeight = (GAME_VIEWPORT_HEIGHT - cameraHeight * cameraZoom) / 2;
			float v_FrustWidth = (GAME_VIEWPORT_WIDTH - cameraWidth * cameraZoom) / 2;
			float v_FrustHeight = (cameraHeight * cameraZoom) + GAME_VIEWPORT_HEIGHT;
			SDL_Texture* frustrumTexture = frustrum->getTexture();
			bool _frustScales = false;
			Vector2 frustrumScale = { 1, 1 };
			// Offsets in px
			Vector2 topFrustOffset = Vector2(v_FrustWidth, h_FrustHeight);
			Vector2 botFrustOffset = Vector2(v_FrustWidth, 0);
			Vector2 leftFrustOffset = Vector2(v_FrustWidth, v_FrustHeight);
			Vector2 rightFrustOffset = Vector2(0, v_FrustHeight);
			float redValue = trunc(frustrumColor.x);
			float greenValue = trunc(frustrumColor.y);
			float blueValue = trunc(frustrumColor.z);
			float alphaValue = trunc(frustrumColor.w);
			ImU32 frustrumColorU32 = (((ImU32)(alphaValue) << 24) | ((ImU32)(blueValue) << 16) | ((ImU32)(greenValue) << 8) | ((ImU32)(redValue)));

			// Set drawing channel to maxSpriteLayers + 2 for top layer camera UI
			drawSplitter->SetCurrentChannel(draw_list, maxSpriteLayers + 2);

			// Top frustrum
			//AddImageToDrawList(frustrumTexture, Vector2(-cameraWidth / 2, cameraHeight / 2), viewportCenterPoint, h_FrustWidth, h_FrustHeight, topFrustOffset, frustrumScale, _frustScales, cameraZoom, draw_list, frustrumColorU32);
			// Bottom frustrum
			//AddImageToDrawList(frustrumTexture, Vector2(-cameraWidth / 2, -cameraHeight / 2), viewportCenterPoint, h_FrustWidth, h_FrustHeight, botFrustOffset, frustrumScale, _frustScales, cameraZoom, draw_list, frustrumColorU32);
			// Left frustrum
			//AddImageToDrawList(frustrumTexture, Vector2(-cameraWidth / 2, -cameraHeight / 2), viewportCenterPoint, v_FrustWidth, v_FrustHeight, leftFrustOffset, frustrumScale, _frustScales, cameraZoom, draw_list, frustrumColorU32);
			// Left frustrum
			//AddImageToDrawList(frustrumTexture, Vector2(cameraWidth / 2, -cameraHeight / 2), viewportCenterPoint, v_FrustWidth, v_FrustHeight, rightFrustOffset, frustrumScale, _frustScales, cameraZoom, draw_list, frustrumColorU32);

			drawSplitter->Merge(draw_list);
		}
	}

	ImVec2 Game_GetTotalCameraOffset(std::shared_ptr<Camera> primaryCamera)
	{
		std::shared_ptr<GameObject> parent = FlatEngine::GetObjectById(primaryCamera->GetParentID());
		std::shared_ptr<Transform> transform = std::static_pointer_cast<Transform>(parent->GetComponent(ComponentTypes::Transform));
		Vector2 offset = transform->GetPosition();
		Vector2 scale = transform->GetScale();

		// If the Primary Camera has a parent, get its offset recursively
		if (parent->GetParentID() != -1)
			Game_GetTotalOffsetAndScale(parent, offset, scale);

		return ImVec2(offset.x, offset.y);
	}

	void Game_GetTotalOffsetAndScale(std::shared_ptr<GameObject> child, Vector2& offset, Vector2& scale)
	{
		// Check if the child actually has a parent object
		if (child->GetParentID() != -1)
		{
			std::shared_ptr<GameObject> parent = FlatEngine::GetObjectById(child->GetParentID());
			std::shared_ptr<Transform> parentTransform = std::static_pointer_cast<Transform>(parent->GetComponent(ComponentTypes::Transform));
			Vector2 parentPosition = Vector2(0, 0);
			Vector2 parentScale = Vector2(1, 1);

			if (parentTransform != nullptr)
			{
				parentPosition = parentTransform->GetPosition();
				parentScale = parentTransform->GetScale();
			}

			offset.x += parentPosition.x;
			offset.y += parentPosition.y;
			scale.x *= parentScale.x;
			scale.y *= parentScale.y;

			// Check if the childs parent has a parent
			if (parent->GetParentID() != -1)
				Game_GetTotalOffsetAndScale(parent, offset, scale);
		}
	}

	// Helper - Recursively draws scene objects and their children to the game view
	void Game_RenderSelfThenChildren(std::shared_ptr<GameObject> self, Vector2 parentOffset, Vector2 parentScale, ImVec2 worldCenterPoint, ImVec2 canvas_p0,
		ImVec2 canvas_sz, ImDrawList* draw_list, ImDrawListSplitter* drawSplitter, ImVec2 cameraPosition, float cameraWidth, float cameraHeight, float cameraZoom)
	{
		// Get Components
		std::shared_ptr<Component> transformComponent = self->GetComponent(Component::ComponentTypes::Transform);
		std::shared_ptr<Component> spriteComponent = self->GetComponent(Component::ComponentTypes::Sprite);
		std::shared_ptr<Component> animationComponent = self->GetComponent(Component::ComponentTypes::Animation);
		std::shared_ptr<Component> textComponent = self->GetComponent(ComponentTypes::Text);
		std::shared_ptr<Component> buttonComponent = self->GetComponent(ComponentTypes::Button);

		// Animation component handling
		if (animationComponent != nullptr)
		{
			// Cast Animation component
			std::shared_ptr<Animation> animationCasted = std::static_pointer_cast<Animation>(animationComponent);

			// If animation component is playing, play the animation
			if (animationCasted != nullptr && animationCasted->IsPlaying())
				animationCasted->PlayAnimation(animationTime);
		}


		// Check if each object has a Transform component
		if (transformComponent != nullptr)
		{
			long focusedObjectID = FlatEngine::GetFocusedGameObjectID();
			std::shared_ptr<Transform> transformCasted = std::static_pointer_cast<Transform>(transformComponent);
			Vector2 position = Vector2(transformCasted->GetPosition().x + parentOffset.x, transformCasted->GetPosition().y + parentOffset.y);
			Vector2 scale = Vector2(transformCasted->GetScale().x * parentScale.x, transformCasted->GetScale().y * parentScale.y);

			// If it has a text component, render that text texture at the objects transform position
			if (textComponent != nullptr)
			{
				// Cast the component to Text shared_ptr
				std::shared_ptr<Text> textCasted = std::static_pointer_cast<Text>(textComponent);
				std::shared_ptr<Texture> textTexture = textCasted->GetTexture();
				textCasted->LoadText();
				SDL_Texture* texture = textTexture->getTexture();
				float textWidth = (float)textTexture->getWidth();
				float textHeight = (float)textTexture->getHeight();
				int renderOrder = textCasted->GetRenderOrder();
				Vector2 offset = textCasted->GetOffset();
				bool _spriteScalesWithZoom = true;


				// If there is a valid Texture loaded into the Sprite Component
				if (textTexture != nullptr)
				{
					// Change the draw channel for the scene object
					if (renderOrder <= maxSpriteLayers && renderOrder >= 0)
						drawSplitter->SetCurrentChannel(draw_list, renderOrder);
					else
						drawSplitter->SetCurrentChannel(draw_list, 0);

					// Draw the texture
					AddImageToDrawList(textTexture->getTexture(), position, worldCenterPoint, textWidth, textHeight, offset, scale, _spriteScalesWithZoom, cameraZoom, draw_list);
				}
			}

			// If it has a sprite component, render that sprite texture at the objects transform position
			if (spriteComponent != nullptr)
			{
				// Cast the components to their respective types
				std::shared_ptr<Sprite> spriteCasted = std::static_pointer_cast<Sprite>(spriteComponent);

				SDL_Texture* spriteTexture = spriteCasted->GetTexture();
				float textureWidth = (float)spriteCasted->GetTextureWidth();
				float textureHeight = (float)spriteCasted->GetTextureHeight();
				Vector2 offset = spriteCasted->GetOffset();
				bool _scalesWithZoom = true;
				int renderOrder = spriteCasted->GetRenderOrder();

				// Changing the scale here because things are rendering too large and I want them to start off smaller
				Vector2 newScale = Vector2(scale.x * spriteScaleMultiplier, scale.y * spriteScaleMultiplier);

				float spriteLeftEdge = position.x - offset.x * newScale.x;
				float spriteRightEdge = position.x + offset.x * newScale.x;
				float spriteTopEdge = position.y + offset.y * newScale.y;
				float spriteBottomEdge = position.y - offset.y * newScale.y;

				float cameraLeftEdge = cameraPosition.x - cameraWidth / 2;
				float cameraRightEdge = cameraPosition.x + cameraWidth / 2;
				float cameraTopEdge = cameraPosition.y + cameraHeight / 2;
				float cameraBottomEdge = cameraPosition.y - cameraHeight / 2;

				bool _isIntersecting = false;

				if (spriteLeftEdge < cameraRightEdge && spriteRightEdge > cameraLeftEdge &&
					spriteTopEdge > cameraBottomEdge && spriteBottomEdge < cameraTopEdge)
					_isIntersecting = true;

				if (_isIntersecting)
				{
					if (renderOrder <= maxSpriteLayers && renderOrder >= 0)
						drawSplitter->SetCurrentChannel(draw_list, renderOrder);
					else
						drawSplitter->SetCurrentChannel(draw_list, 0);
					AddImageToDrawList(spriteTexture, position, worldCenterPoint, textureWidth, textureHeight, offset, scale, _scalesWithZoom, cameraZoom, draw_list);
				}
			}

			// Renders Button Component
			if (buttonComponent != nullptr)
			{
				std::shared_ptr<Button> button = std::static_pointer_cast<Button>(buttonComponent);

				float activeWidth = button->GetActiveWidth();
				float activeHeight = button->GetActiveHeight();
				Vector2 activeOffset = button->GetActiveOffset();
				bool _isActive = button->IsActive();

				// Get Active Button bounds to check against later for mouse events
				float activeLeft = WorldToViewport(worldCenterPoint.x, position.x + activeOffset.x - (activeWidth / 2 * scale.x), cameraZoom, false);
				float activeRight = WorldToViewport(worldCenterPoint.x, position.x + activeOffset.x + (activeWidth / 2 * scale.x), cameraZoom, false);
				float activeTop = WorldToViewport(worldCenterPoint.y, position.y + activeOffset.y + (activeHeight / 2 * scale.y), cameraZoom, true);
				float activeBottom = WorldToViewport(worldCenterPoint.y, position.y + activeOffset.y - (activeHeight / 2 * scale.y), cameraZoom, true);

				button->SetActiveEdges(ImVec4(activeTop, activeRight, activeBottom, activeLeft));

				// FOR DRAWING ACTIVE BUTTON RECTANGLE IN GAME VIEW
				// 
				//Vector2 topLeft = { activeLeft, activeTop };
				//Vector2 bottomRight = { activeRight, activeBottom };

				//drawSplitter->SetCurrentChannel(draw_list, maxSpriteLayers + 2);

				//if (_isActive)
				//	FlatEngine::DrawRectangle(topLeft, bottomRight, canvas_p0, canvas_sz, FlatEngine::ActiveButtonColor, 3.0f, draw_list);
				//else
				//	FlatEngine::DrawRectangle(topLeft, bottomRight, canvas_p0, canvas_sz, FlatEngine::InactiveButtonColor, 3.0f, draw_list);
			}
		}

		if (self->HasChildren())
		{
			if (transformComponent != nullptr)
			{
				std::shared_ptr<Transform> transformCasted = std::static_pointer_cast<Transform>(transformComponent);
				parentOffset.x += transformCasted->GetPosition().x;
				parentOffset.y += transformCasted->GetPosition().y;
				parentScale.x *= transformCasted->GetScale().x;
				parentScale.y *= transformCasted->GetScale().y;
			}

			for (int c = 0; c < self->GetChildren().size(); c++)
			{
				std::shared_ptr<GameObject> child = FlatEngine::GetObjectById(self->GetChildren()[c]);

				if (child->IsActive())
					Game_RenderSelfThenChildren(child, parentOffset, parentScale, worldCenterPoint, canvas_p0, canvas_sz, draw_list, drawSplitter,
						cameraPosition, cameraWidth, cameraHeight, cameraZoom);
			}
		}
	}
}
}