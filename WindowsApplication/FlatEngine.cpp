#include "FlatEngine.h"
#include "UIManager.h"
#include "Logger.h"
#include "Scene.h"
#include "Transform.h"
#include "Sprite.h"
#include "BoxCollider.h"
#include "ScriptComponent.h"
#include "Vector2.h"
#include <fstream>
#include <string>
#include <iostream>
#include <filesystem>
#include <cmath>
#include "MappingContext.h"
#include "PrefabManager.h"

/*
######################################
######							######
######         Engine           ######
######                          ######
######################################
*/





namespace FlatEngine
{
	bool _isDebugMode = true;
	bool _closeProgram = false;

	std::vector<SDL_Joystick*> gamepads = std::vector<SDL_Joystick*>();
	int JOYSTICK_DEAD_ZONE = 4000;


	// Managers
	std::shared_ptr<GameManager> FlatEngine::gameManager = nullptr;
	std::shared_ptr<Sound> soundController = std::make_shared<Sound>();
	long FlatEngine::FocusedGameObjectID = -1;
	FlatEngine::SceneManager* FlatEngine::sceneManager = new FlatEngine::SceneManager();
	FlatEngine::Logger* FlatEngine::logger = new FlatEngine::Logger();
	FlatEngine::GameLoop* FlatEngine::gameLoop = new FlatEngine::GameLoop();
	std::shared_ptr<FlatEngine::FlatGui::WidgetsManager> widgetsManager(new FlatEngine::FlatGui::WidgetsManager());
	std::shared_ptr<FlatEngine::FlatGui::UIManager> uiManager(new FlatEngine::FlatGui::UIManager());
	std::vector<std::shared_ptr<RigidBody>> rigidBodies = std::vector<std::shared_ptr<RigidBody>>();
	std::vector<std::shared_ptr<Collider>> colliders = std::vector<std::shared_ptr<Collider>>();	
	std::vector<std::pair<std::shared_ptr<Collider>, std::shared_ptr<Collider>>> colliderPairs = std::vector<std::pair<std::shared_ptr<Collider>, std::shared_ptr<Collider>>>();
	std::shared_ptr<PrefabManager> prefabManager = std::make_shared<PrefabManager>();
	std::shared_ptr<GameObject> playerObject = nullptr;

	// Loaded Project
	std::shared_ptr<Project> loadedProject = std::make_shared<Project>();

	// Mapping Context Management
	std::vector<std::shared_ptr<MappingContext>> mappingContexts;

	// Animator
	std::shared_ptr<Animation::S_AnimationProperties> FocusedAnimation = std::make_shared<Animation::S_AnimationProperties>();
	std::shared_ptr<GameObject> objectForFocusedAnimation = nullptr;
	std::shared_ptr<Animation::S_Property> selectedKeyFrameToEdit = nullptr;
	int previewAnimationStartTime = 0;
	int previewAnimationTime = 0;
	bool _playPreviewAnimation = true;

	// Profiler
	std::vector<std::shared_ptr<Process>> profilerProcesses = std::vector<std::shared_ptr<Process>>();

	// FlatEngine
	void Run(bool& _hasQuit)
	{
		// Manage Controllers
		static int controllersConnected = 0;
		if (SDL_NumJoysticks() != controllersConnected)
		{
			// Clean up old gamepads
			for (SDL_Joystick* gamepad : gamepads)
			{
				SDL_JoystickClose(gamepad);
				gamepad = NULL;
			}

			controllersConnected = SDL_NumJoysticks();
			for (int i = 0; i < controllersConnected; i++)
			{
				SDL_Joystick* gamepad = SDL_JoystickOpen(i);
				if (gamepad == NULL)
					printf("Warning: Unable to open game controller! SDL Error: %s\n", SDL_GetError());
				else
				{
					LogString("Gamepad connected...");
					gamepads.push_back(gamepad);
				}
			}
		}

		float renderStartTime = 0;
		static double frameStart = GetEngineTime();

		if (_isDebugMode)
			renderStartTime = (float)GetEngineTime(); // Profiler
		_hasQuit = _closeProgram;
		FlatGui::Render(_hasQuit);
		if (_isDebugMode)
			AddProcessData("Render", (float)GetEngineTime() - renderStartTime); // Profiler


		//// If Release - Start the Game Loop
		if (!_isDebugMode && !GameLoopStarted())
			StartGameLoop();


		if (GameLoopStarted() && !GameLoopPaused() || GameLoopPaused() && gameLoop->IsFrameSkipped())
		{
			// Profiler
			float updateLoopStart = 0;
			static float updateLoopEnd = 0;
			if (_isDebugMode)
			{
				// Save time before Update starts
				updateLoopStart = (float)GetEngineTime();
				// Get hang time of everything after Update Loop for profiler
				float everythingElseHangTime = updateLoopStart - updateLoopEnd;
				AddProcessData("Not GameLoop", everythingElseHangTime);
				updateLoopEnd = updateLoopStart;
			}

			double frameTime = (GetEngineTime() - frameStart) / 1000; // actual deltaTime (in seconds)

			// Physics Update Version 1
			if (!gameLoop->IsGamePaused())
				gameLoop->accumulator += frameTime;

			if (!gameLoop->IsGamePaused())
			{
				while (gameLoop->accumulator >= gameLoop->deltaTime)
				{
					//float t0 = GetEngineTime();
					FlatGui::HandleEvents(_hasQuit);
					GameLoopUpdate();
					gameLoop->SetFrameSkipped(false);

					gameLoop->time += gameLoop->deltaTime;
					gameLoop->accumulator -= gameLoop->deltaTime;
					//float t1 = GetEngineTime() - t0;
					//LogFloat(t1, "T1: ");
				}
			}

			// Get time it took to get back to GameLoopUpdate()
			frameStart = (double)GetEngineTime();

			// Artificially slow GameLoop if frameTime is less than 
			if (!loadedProject->IsVsyncEnabled() && frameTime < GetDeltaTime())
				SDL_Delay(GetDeltaTime() - frameTime);

			// Profiler
			if (_isDebugMode)
			{
				// Get hang time of Update Loop for profiler
				float hangTime = (float)GetEngineTime() - updateLoopStart;
				AddProcessData("GameLoop (variable executions)", hangTime);
				// Save time after update finishes
				updateLoopEnd = (float)GetEngineTime();
			}

			// Physics update V2
			{
				// Physics Update Version 2
				// Clamp cycles per Run loop (avoids physics death spiral)
				//int maxCyclesPerRunLoop = 100;
				//int cycleCounter = 0;
				//if (frameTime < gameLoop->deltaTime)
				//{
				//	GameLoopUpdate();
				//	gameLoop->SetFrameSkipped(false);
				//	float dt = std::min<double>(frameTime, gameLoop->deltaTime);
				//	frameTime -= dt;
				//	gameLoop->time += gameLoop->deltaTime;
				//}
				//else if (cycleCounter < maxCyclesPerRunLoop)
				//{
				//	while (frameTime > 0.0)
				//	{
				//		GameLoopUpdate();
				//		gameLoop->SetFrameSkipped(false);
				//		float dt = std::min<double>(frameTime, gameLoop->deltaTime);
				//		frameTime -= dt;
				//		gameLoop->time += gameLoop->deltaTime;
				//		
				//	}
				//	cycleCounter++;
				//}
			}
		}
		else
			FlatGui::HandleEvents(_hasQuit);

		// If gameloop isn't running, make sure our framestart keeps up with current engine time otherwise it will cause a stutter on first starting gameloop
		if (!gameLoop->IsStarted())
			frameStart = GetEngineTime();

		FlatGui::RenderClear();
	}

	void CloseProgram()
	{
		// Clean up old gamepads
		for (SDL_Joystick* gamepad : gamepads)
		{
			SDL_JoystickClose(gamepad);
			gamepad = NULL;
		}

		FlatEngine::_closeProgram = true;
	}

	Uint32 GetEngineTime()
	{
		return (int)SDL_GetTicks();
	}

	json LoadFileData(std::string filename)
	{
		// Declare file and input stream
		std::ofstream file_obj;
		std::ifstream ifstream(filename);

		// Open file in in mode
		file_obj.open(filename, std::ios::in);

		// Variable to save the current file data into
		std::string fileContent = "";

		// Loop through the file line by line and save the data
		if (file_obj.good())
		{
			std::string line;
			while (!ifstream.eof()) {
				std::getline(ifstream, line);
				fileContent.append(line + "\n");
			}
		}

		// Close the file after reading
		file_obj.close();

		if (file_obj.good() && fileContent != "\n")
		{
			// Go from string to json object
			return json::parse(fileContent);
		}
		else
			return NULL;
	}

	void SetFocusedGameObjectID(long ID)
	{
		FocusedGameObjectID = ID;
		if (ID != -1)
		{
			std::shared_ptr<GameObject> focusedObject = GetObjectById(ID);
			std::shared_ptr<Animation> animationComponent = focusedObject->GetAnimationComponent();
			std::string animationPath = "";

			if (animationComponent != nullptr)
				animationPath = animationComponent->GetAnimationPath();
	
			// If applicable to the current animation, create a copy of the focused GameObject to be used for the animator window.
			if (FlatGui::_showAnimator && FocusedAnimation != nullptr &&
				animationComponent != nullptr && animationPath == FocusedAnimation->animationPath)
			{
				std::vector<std::shared_ptr<GameObject>> animatorObjects = std::vector<std::shared_ptr<GameObject>>();
				animatorObjects.clear();
				objectForFocusedAnimation = std::make_shared<GameObject>(GetObjectById(ID), animatorObjects, GetLoadedScene()->GetSceneObjects(), -1);
				std::shared_ptr<Transform> transform = objectForFocusedAnimation->GetTransformComponent();
				transform->SetPosition(Vector2(0,0));
				animatorObjects.push_back(objectForFocusedAnimation);
				GetLoadedScene()->SetAnimatorPreviewObjects(animatorObjects);
			}
		}
	}

	long GetFocusedGameObjectID()
	{
		return FlatEngine::FocusedGameObjectID;
	}

	void SetFocusedAnimation(std::shared_ptr<Animation::S_AnimationProperties> animation)
	{
		FocusedAnimation = animation;
	}

	std::shared_ptr<Animation::S_AnimationProperties> GetFocusedAnimation()
	{
		return FocusedAnimation;
	}

	void DestroySelf(std::shared_ptr<FlatEngine::GameObject> thisObject)
	{
		FlatEngine::DeleteGameObject(thisObject->GetID());
	}

	void DestroySelfAndParent(std::shared_ptr<FlatEngine::GameObject> thisObject)
	{
		FlatEngine::DeleteGameObject(thisObject->GetParentID());
	}
	
	// Project Management
	void OpenProject(std::string path)
	{
		std::shared_ptr<Project> newProject = std::make_shared<Project>();
		newProject->SetPath(path);

		// Declare file and input stream
		std::ofstream file_obj;
		std::ifstream ifstream(path);

		// Open file in in mode
		file_obj.open(path, std::ios::in);

		// Variable to save the current file data into
		std::string fileContent = "";

		// Loop through the file line by line and save the data
		if (file_obj.good())
		{
			std::string line;
			while (!ifstream.eof()) {
				std::getline(ifstream, line);
				fileContent.append(line + "\n");
			}
		}

		// Close the file after reading
		file_obj.close();

		if (file_obj.good())
		{
			// Go from string to json object
			json fileContentJson = json::parse(fileContent);

			if (fileContentJson["Project Properties"][0] != "NULL")
			{
				// Getting data from the json 
				// auto properties = fileContentJson["Animation Properties"];
				// std::string name = properties[0]["name"];

				// Loop through the saved Properties in the JSON file
				for (int i = 0; i < fileContentJson["Project Properties"].size(); i++)
				{
					// Get data from the loaded object
					json currentObjectJson = fileContentJson["Project Properties"][i];

					// Open items
					if (currentObjectJson.contains("path"))
						newProject->SetPath(currentObjectJson["path"]);
					if (currentObjectJson.contains("loadedScenePath"))
						newProject->SetLoadedScenePath(currentObjectJson["loadedScenePath"]);
					if (currentObjectJson.contains("loadedAnimationPath"))
						newProject->SetLoadedPreviewAnimationPath(currentObjectJson["loadedAnimationPath"]);
					if (currentObjectJson.contains("focusedGameObjectID"))
						newProject->SetFocusedGameObjectID(currentObjectJson["focusedGameObjectID"]);

					// Scene Scrolling + Grid Step
					Vector2 sceneViewScroll = Vector2(0, 0);
					if (currentObjectJson.contains("sceneViewScrollingX"))				
						sceneViewScroll.x = currentObjectJson["sceneViewScrollingX"];
					if (currentObjectJson.contains("sceneViewScrollingY"))
						sceneViewScroll.y = currentObjectJson["sceneViewScrollingY"];
					newProject->SetSceneViewScrolling(sceneViewScroll);
					Vector2 sceneViewGridStep = Vector2(0, 0);
					if (currentObjectJson.contains("sceneViewGridStepX"))
						sceneViewGridStep.x = currentObjectJson["sceneViewGridStepX"];
					if (currentObjectJson.contains("sceneViewGridStepY"))
						sceneViewGridStep.y = currentObjectJson["sceneViewGridStepY"];
					newProject->SetSceneViewGridStep(sceneViewGridStep);

					// Show/hide windows
					if (currentObjectJson.contains("_showSceneView"))
						FlatGui::_showSceneView = currentObjectJson["_showSceneView"];
					if (currentObjectJson.contains("_showGameView"))
						FlatGui::_showGameView = currentObjectJson["_showGameView"];
					if (currentObjectJson.contains("_showHierarchy"))
						FlatGui::_showHierarchy = currentObjectJson["_showHierarchy"];
					if (currentObjectJson.contains("_showInspector"))
						FlatGui::_showInspector = currentObjectJson["_showInspector"];
					if (currentObjectJson.contains("_showAnimator"))
						FlatGui::_showAnimator = currentObjectJson["_showAnimator"];
					if (currentObjectJson.contains("_showAnimationPreview"))
						FlatGui::_showAnimationPreview = currentObjectJson["_showAnimationPreview"];
					if (currentObjectJson.contains("_showKeyFrameEditor"))
						FlatGui::_showKeyFrameEditor = currentObjectJson["_showKeyFrameEditor"];
					if (currentObjectJson.contains("_showLogger"))
						FlatGui::_showLogger = currentObjectJson["_showLogger"];
					if (currentObjectJson.contains("_showProfiler"))
						FlatGui::_showProfiler = currentObjectJson["_showProfiler"];
					if (currentObjectJson.contains("_showMappingContextEditor"))
						FlatGui::_showMappingContextEditor = currentObjectJson["_showMappingContextEditor"];

					// Settings
					if (currentObjectJson.contains("_clearLogBuffer"))
					{
						FlatGui::_clearBufferEveryFrame = currentObjectJson["_clearLogBuffer"];
						FlatEngine::logger->ClearBuffer();
					}
					if (currentObjectJson.contains("_autoSave"))
						newProject->SetAutoSave(currentObjectJson["_autoSave"]);
					if (currentObjectJson.contains("physicsSystem"))
						newProject->SetPhysicsSystem(currentObjectJson["physicsSystem"]);
					if (currentObjectJson.contains("collisionDetection"))
						newProject->SetCollisionDetection(currentObjectJson["collisionDetection"]);
					if (currentObjectJson.contains("resolutionWidth") && currentObjectJson.contains("resolutionHeight"))
						newProject->SetResolution(Vector2(currentObjectJson["resolutionWidth"], currentObjectJson["resolutionHeight"]));
					if (currentObjectJson.contains("_fullscreen"))
						newProject->SetFullscreen(currentObjectJson["_fullscreen"]);
					if (currentObjectJson.contains("_vsyncEnabled"))
						newProject->SetVsyncEnabled(currentObjectJson["_vsyncEnabled"]);
				}
			}
		}

		loadedProject = newProject;
		
		if (loadedProject->GetLoadedScenePath() != "")
			LoadScene(loadedProject->GetLoadedScenePath());
		else
			CreateNewScene();
		if (loadedProject->GetLoadedPreviewAnimationPath() != "")
			SetFocusedAnimation(FlatGui::LoadAnimationFile(loadedProject->GetLoadedPreviewAnimationPath()));
		Vector2 scrolling = loadedProject->GetSceneViewScrolling();
		FlatGui::sceneViewScrolling = scrolling;
		Vector2 gridStep = loadedProject->GetSceneViewGridStep();
		FlatGui::sceneViewGridStep = gridStep;

		if (loadedProject->GetFocusedGameObjectID() != -1 && GetObjectById(loadedProject->GetFocusedGameObjectID()) != nullptr)
			SetFocusedGameObjectID(loadedProject->GetFocusedGameObjectID());
	}

	void SaveProject(std::shared_ptr<Project> project, std::string path)
	{
		// Declare file and input stream
		std::ofstream file_obj;
		std::ifstream ifstream(path);

		// Delete old contents of the file
		file_obj.open(path, std::ofstream::out | std::ofstream::trunc);
		file_obj.close();

		// Opening file in append mode
		file_obj.open(path, std::ios::app);

		// Array that will hold our gameObject json objects
		json projectProperties;

		// Create Animation Property Json data object
		json animationName = json::object({
			{ "path", path },
			{ "loadedScenePath", project->GetLoadedScenePath()},
			{ "loadedAnimationPath", project->GetLoadedPreviewAnimationPath()},
			{ "focusedGameObjectID", GetFocusedGameObjectID() },
			{ "sceneViewScrollingX", FlatGui::sceneViewScrolling.x },
			{ "sceneViewScrollingY", FlatGui::sceneViewScrolling.y },
			{ "sceneViewGridStepX", FlatGui::sceneViewGridStep.x },
			{ "sceneViewGridStepY", FlatGui::sceneViewGridStep.y },
			{ "_showSceneView", FlatGui::_showSceneView },
			{ "_showGameView", FlatGui::_showGameView },
			{ "_showHierarchy", FlatGui::_showHierarchy },
			{ "_showInspector", FlatGui::_showInspector },
			{ "_showAnimator", FlatGui::_showAnimator },
			{ "_showAnimationPreview", FlatGui::_showAnimationPreview },
			{ "_showKeyFrameEditor", FlatGui::_showKeyFrameEditor },
			{ "_showLogger", FlatGui::_showLogger },
			{ "_showProfiler", FlatGui::_showProfiler },
			{ "_showMappingContextEditor", FlatGui::_showMappingContextEditor },
			{ "_clearLogBuffer", FlatGui::_clearBufferEveryFrame },
			{ "_autoSave", loadedProject->AutoSaveOn() },
			{ "physicsSystem", loadedProject->GetPhysicsSystem() },
			{ "collisionDetection", loadedProject->GetCollisionDetection() },
			{ "resolutionWidth", loadedProject->GetResolution().x },
			{ "resolutionHeight", loadedProject->GetResolution().y },
			{ "_fullscreen", loadedProject->IsFullscreen() },
			{ "_vsyncEnabled", loadedProject->IsVsyncEnabled() },
		});
		projectProperties.push_back(animationName);

		// Recreate the Animation Property json object and add the array as the content
		json newFileObject = json::object({ {"Project Properties", projectProperties } });

		// Add the GameObjects object contents to the file
		file_obj << newFileObject.dump(4).c_str() << std::endl;

		// Close the file
		file_obj.close();
	}


	// Mapping Context Management
	void SaveMappingContext(std::string path, std::shared_ptr<MappingContext> context)
	{
		// Declare file and input stream
		std::ofstream file_obj;
		std::ifstream ifstream(path);

		// Delete old contents of the file
		file_obj.open(path, std::ofstream::out | std::ofstream::trunc);
		file_obj.close();

		// Opening file in append mode
		file_obj.open(path, std::ios::app);

		// Array that will hold our mappings json objects
		json mappings = json::array();

		if (context->GetKeyBindings().size() > 0)
		{
			std::string data = context->GetData();
			mappings.push_back(json::parse(data));
		}
		else
			mappings.push_back("NULL");

		// Recreate the json object and add the array as the content
		json newFileObject = json::object({ {"Mapping Context", mappings } });

		// Add the GameObjects object contents to the file
		file_obj << newFileObject.dump(4).c_str() << std::endl;

		// Close the file
		file_obj.close();
	}

	void InitializeMappingContexts()
	{
		mappingContexts.clear();

		std::string path = "C:\\Users\\Dillon Kyle\\source\\repos\\FlatEngine\\WindowsApplication\\mappingContext";
		for (const auto& entry : std::filesystem::directory_iterator(path))
		{
			// Create a new context to save the loaded keybindings to
			std::shared_ptr<MappingContext> newContext = std::make_shared<MappingContext>();

			json contextData = LoadFileData(entry.path().string());
			if (contextData != NULL)
			{
				//Getting data from the json 
				//std::string name = firstObjectName[0]["name"];

				auto mappings = contextData["Mapping Context"][0];
				
				newContext->SetName(mappings["name"]);
				newContext->SetPath(entry.path().string());
				
				// XInput
				newContext->AddKeyBinding("XInput_A", mappings["XInput_A"]);
				newContext->AddKeyBinding("XInput_B", mappings["XInput_B"]);
				newContext->AddKeyBinding("XInput_X", mappings["XInput_X"]);
				newContext->AddKeyBinding("XInput_Y", mappings["XInput_Y"]);
				newContext->AddKeyBinding("XInput_LB", mappings["XInput_LB"]);
				newContext->AddKeyBinding("XInput_RB", mappings["XInput_RB"]);
				newContext->AddKeyBinding("XInput_ScreenShot", mappings["XInput_ScreenShot"]);
				newContext->AddKeyBinding("XInput_Start", mappings["XInput_Start"]);
				newContext->AddKeyBinding("XInput_LS", mappings["XInput_LS"]);
				newContext->AddKeyBinding("XInput_RS", mappings["XInput_RS"]);
				newContext->AddKeyBinding("XInput_Home", mappings["XInput_Home"]);
				newContext->AddKeyBinding("XInput_Tray", mappings["XInput_Tray"]);
				newContext->AddKeyBinding("XInput_DPadUp", mappings["XInput_DPadUp"]);
				newContext->AddKeyBinding("XInput_DPadDown", mappings["XInput_DPadDown"]);
				newContext->AddKeyBinding("XInput_DPadLeft", mappings["XInput_DPadLeft"]);
				newContext->AddKeyBinding("XInput_DPadRight", mappings["XInput_DPadRight"]);
				newContext->AddKeyBinding("XInput_LeftJoystickX", mappings["XInput_LeftJoystickX"]);
				newContext->AddKeyBinding("XInput_LeftJoystickY", mappings["XInput_LeftJoystickY"]);
				newContext->AddKeyBinding("XInput_RightJoystick", mappings["XInput_RightJoystick"]);
				newContext->AddKeyBinding("XInput_LT", mappings["XInput_LT"]);
				newContext->AddKeyBinding("XInput_RT", mappings["XInput_RT"]);

				//// Keyboard + Mouse
				// Directional
				newContext->AddKeyBinding("SDLK_UP", mappings["SDLK_UP"]);
				newContext->AddKeyBinding("SDLK_DOWN", mappings["SDLK_DOWN"]);
				newContext->AddKeyBinding("SDLK_LEFT", mappings["SDLK_LEFT"]);
				newContext->AddKeyBinding("SDLK_RIGHT", mappings["SDLK_RIGHT"]);
				// Letters
				newContext->AddKeyBinding("SDLK_a", mappings["SDLK_a"]);
				newContext->AddKeyBinding("SDLK_b", mappings["SDLK_b"]);
				newContext->AddKeyBinding("SDLK_c", mappings["SDLK_c"]);
				newContext->AddKeyBinding("SDLK_d", mappings["SDLK_d"]);
				newContext->AddKeyBinding("SDLK_e", mappings["SDLK_e"]);
				newContext->AddKeyBinding("SDLK_f", mappings["SDLK_f"]);
				newContext->AddKeyBinding("SDLK_g", mappings["SDLK_g"]);
				newContext->AddKeyBinding("SDLK_h", mappings["SDLK_h"]);
				newContext->AddKeyBinding("SDLK_i", mappings["SDLK_i"]);
				newContext->AddKeyBinding("SDLK_j", mappings["SDLK_j"]);
				newContext->AddKeyBinding("SDLK_k", mappings["SDLK_k"]);
				newContext->AddKeyBinding("SDLK_l", mappings["SDLK_l"]);
				newContext->AddKeyBinding("SDLK_m", mappings["SDLK_m"]);
				newContext->AddKeyBinding("SDLK_n", mappings["SDLK_n"]);
				newContext->AddKeyBinding("SDLK_o", mappings["SDLK_o"]);
				newContext->AddKeyBinding("SDLK_p", mappings["SDLK_p"]);
				newContext->AddKeyBinding("SDLK_q", mappings["SDLK_q"]);
				newContext->AddKeyBinding("SDLK_r", mappings["SDLK_r"]);
				newContext->AddKeyBinding("SDLK_s", mappings["SDLK_s"]);
				newContext->AddKeyBinding("SDLK_t", mappings["SDLK_t"]);
				newContext->AddKeyBinding("SDLK_u", mappings["SDLK_u"]);
				newContext->AddKeyBinding("SDLK_v", mappings["SDLK_v"]);
				newContext->AddKeyBinding("SDLK_w", mappings["SDLK_w"]);
				newContext->AddKeyBinding("SDLK_x", mappings["SDLK_x"]);
				newContext->AddKeyBinding("SDLK_y", mappings["SDLK_y"]);
				newContext->AddKeyBinding("SDLK_z", mappings["SDLK_z"]);

				// After all keys are set, create their Input Action bindings
				newContext->CreateInputActionBindings();

				// Add context to context managing vector
				mappingContexts.push_back(newContext);
			}
		}
	}

	std::shared_ptr<MappingContext> GetMappingContext(std::string contextName)
	{
		for (std::shared_ptr<MappingContext> mappingContext : mappingContexts)
			if (mappingContext->GetName() == contextName)
				return mappingContext;

		return nullptr;
	}
	

	// Scene Manager Prettification
	std::shared_ptr<Scene> GetLoadedScene()
	{
		return sceneManager->GetLoadedScene();
	}

	std::shared_ptr<Scene> CreateNewScene()
	{
		return FlatEngine::sceneManager->CreateNewScene();
	}

	std::string GetLoadedScenePath()
	{
		return sceneManager->GetLoadedScenePath();
	}

	long GetNextComponentID()
	{
		long nextID = -1;
		std::shared_ptr<Scene> loadedScene = GetLoadedScene();
		if (loadedScene != nullptr)
		{
			nextID = loadedScene->GetNextComponentID();
			GetLoadedScene()->IncrementComponentID();
		}

		return nextID;
	}

	long GetNextGameObjectID()
	{
		long nextID = -1;
		std::shared_ptr<Scene> loadedScene = GetLoadedScene();
		if (loadedScene != nullptr)
		{
			nextID = loadedScene->GetNextGameObjectID();
		}

		return nextID;
	}

	void SaveScene(std::shared_ptr<Scene> scene, std::string filename)
	{
		FlatEngine::sceneManager->SaveScene(scene, filename);
	}

	void LoadScene(std::string name)
	{
		// Stop any playing music
		soundController->StopMusic();
		// Reset buttons in UIManager
		uiManager->ResetButtons();
		if (sceneManager->GetLoadedScene() != nullptr)
			sceneManager->SaveAnimationPreviewObjects();
		sceneManager->LoadScene(name);
		sceneManager->LoadAnimationPreviewObjects();

		// If the GameLoop is running, reinitialize the new scene's GameObjects
		if (GameLoopStarted())
			gameLoop->InitializeScriptObjects(GetSceneObjects());

		if (GetObjectByName("Player") != nullptr)
			playerObject = GetObjectByName("Player");
	}
	

	// Scene Prettification
	std::vector<std::shared_ptr<GameObject>> GetSceneObjects()
	{
		if (FlatEngine::GetLoadedScene() != nullptr)
			return FlatEngine::GetLoadedScene()->GetSceneObjects();
		else
			return std::vector<std::shared_ptr<GameObject>>();
	}

	std::shared_ptr<GameObject> CreateGameObject(long parentID)
	{
		return FlatEngine::GetLoadedScene()->CreateGameObject(parentID);
	}

	void DeleteGameObject(int sceneObjectID)
	{
		FlatEngine::GetLoadedScene()->DeleteGameObject(sceneObjectID);
	}

	std::shared_ptr<Component> GetObjectComponent(long objectID, ComponentTypes type)
	{
		return FlatEngine::GetLoadedScene()->GetObjectById(objectID)->GetComponent(type);
	}

	std::shared_ptr<GameObject> GetObjectById(long objectID)
	{
		return GetLoadedScene()->GetObjectById(objectID);
	}

	std::shared_ptr<GameObject> GetObjectByName(std::string name)
	{
		return FlatEngine::GetLoadedScene()->GetObjectByName(name);
	}


	// Prefabs
	void CreatePrefab(std::string path, std::shared_ptr<GameObject> gameObject)
	{
		prefabManager->CreatePrefab(path, gameObject);
	}

	void InitializePrefabs()
	{
		prefabManager->InitializePrefabs();
	}

	std::shared_ptr<GameObject> Instantiate(std::string prefabName, Vector2 position, long parentID, long ID)
	{
		return prefabManager->Instantiate(prefabName, position, parentID, ID);
	}
	
	std::map<std::string, std::vector<std::shared_ptr<GameObject>>> GetPrefabs()
	{
		return prefabManager->GetPrefabs();
	}


	// Logging Abstraction
	void LogString(std::string line)
	{
		logger->LogString(line);
	}

	void LogSeparator()
	{
		logger->LogSeparator();
	}

	void LogFloat(float var, std::string line)
	{
		logger->LogFloat(var, line);
	}

	void LogInt(int var, std::string line)
	{
		logger->LogInt(var, line);
	}

	void LogVector2(Vector2 vector, std::string line)
	{
		logger->LogVector2(vector, line);
	}

	void DrawRectangle(Vector2 startingPoint, Vector2 endPoint, Vector2 canvas_p0, Vector2 canvas_sz, Vector4 color, float thickness, ImDrawList* drawList)
	{
		if (startingPoint.x < canvas_p0.x)
			startingPoint.x = canvas_p0.x;
		if (endPoint.x > canvas_p0.x + canvas_sz.x)
			endPoint.x = canvas_p0.x + canvas_sz.x;
		if (startingPoint.y < canvas_p0.y)
			startingPoint.y = canvas_p0.y;
		if (endPoint.y > canvas_p0.y + canvas_sz.y)
			endPoint.y = canvas_p0.y + canvas_sz.y;

		logger->DrawRectangle(startingPoint, endPoint, color, thickness, drawList);
	}

	void DebugRectangle(Vector2 startingPoint, Vector2 endPoint, Vector4 color, float thickness, ImDrawList* drawList)
	{
		logger->DrawRectangle(startingPoint, endPoint, color, thickness, drawList);
	}

	void DrawLine(Vector2 startingPoint, Vector2 endingPoint, Vector4 color, float thickness, ImDrawList* drawList)
	{
		logger->DrawLine(startingPoint, endingPoint, color, thickness, drawList);
	}

	void DrawRectangleFromLines(Vector2* corners, Vector4 color, float thickness, ImDrawList* drawList)
	{
		DrawLine(corners[0], corners[1], color, thickness, drawList);
		DrawLine(corners[1], corners[2], color, thickness, drawList);
		DrawLine(corners[2], corners[3], color, thickness, drawList);
		DrawLine(corners[3], corners[0], color, thickness, drawList);
	}

	void DrawCircle(Vector2 center, float radius, Vector4 color, ImDrawList* drawList, float thickness, int segments)
	{
		drawList->AddCircle(center, radius, ImGui::GetColorU32(color), segments, thickness);
	}

	void DrawPoint(Vector2 point, Vector4 color, ImDrawList* drawList)
	{
		logger->DrawPoint(point, color, drawList);
	}


	// Game Loop prettification
	void StartGameLoop()
	{
		gameLoop->Start();
	}

	void GameLoopUpdate()
	{
		gameLoop->Update();
	}

	void PauseGameLoop()
	{
		if (gameLoop->IsPaused())
			gameLoop->Unpause();
		else
			gameLoop->Pause();
	}

	void PauseGame()
	{
		if (gameLoop->IsGamePaused())
			gameLoop->UnpauseGame();
		else
			gameLoop->PauseGame();
	}

	void StopGameLoop()
	{
		gameLoop->Stop();
	}

	long GetEllapsedGameTimeInSec()
	{
		return gameLoop->TimeEllapsedInSec();
	}

	double GetEllapsedGameTimeInMs()
	{
		return gameLoop->TimeEllapsedInMs();
	}

	bool GameLoopStarted()
	{
		return gameLoop->IsStarted();
	}

	bool GameLoopPaused()
	{
		return gameLoop->IsPaused();
	}

	float GetAverageFps()
	{
		return gameLoop->GetAverageFps();
	}

	float GetDeltaTime()
	{
		return gameLoop->GetDeltaTime();
	}

	std::shared_ptr<GameObject> GetPlayerObject()
	{
		return playerObject;
	}

	std::shared_ptr<Camera> GetPrimaryCamera()
	{
		if (GetLoadedScene() != nullptr)
			return GetLoadedScene()->GetPrimaryCamera();
		else
			return nullptr;
	}


	// Helper
	//Vector4 objectA(top, right, bottom, left), Vector4 objectB(top, right, bottom, left)
	bool AreCollidingWorld(Vector4 ObjectA, Vector4 ObjectB)
	{
		float A_TopEdge = ObjectA.x;
		float A_RightEdge = ObjectA.y;
		float A_BottomEdge = ObjectA.z;
		float A_LeftEdge = ObjectA.w;

		float B_TopEdge = ObjectB.x;
		float B_RightEdge = ObjectB.y;
		float B_BottomEdge = ObjectB.z;
		float B_LeftEdge = ObjectB.w;

		return (A_LeftEdge < B_RightEdge && A_RightEdge > B_LeftEdge && A_TopEdge > B_BottomEdge && A_BottomEdge < B_TopEdge);
	}

	bool AreCollidingViewport(Vector4 ObjectA, Vector4 ObjectB)
	{
		float A_TopEdge = ObjectA.z;
		float A_RightEdge = ObjectA.y;
		float A_BottomEdge = ObjectA.x;
		float A_LeftEdge = ObjectA.w;

		float B_TopEdge = ObjectB.x;
		float B_RightEdge = ObjectB.y;
		float B_BottomEdge = ObjectB.z;
		float B_LeftEdge = ObjectB.w;

		return (A_LeftEdge < B_RightEdge && A_RightEdge > B_LeftEdge && A_TopEdge > B_BottomEdge && A_BottomEdge < B_TopEdge);
	}

	bool AreCollidingNoRotation(Vector4 ObjectA, Vector4 ObjectB)
	{
		float A_TopEdge = ObjectA.x;
		float A_RightEdge = ObjectA.y;
		float A_BottomEdge = ObjectA.z;
		float A_LeftEdge = ObjectA.w;

		float B_TopEdge = ObjectB.x;
		float B_RightEdge = ObjectB.y;
		float B_BottomEdge = ObjectB.z;
		float B_LeftEdge = ObjectB.w;

		return ((A_LeftEdge < B_RightEdge) && (A_RightEdge > B_LeftEdge) && (A_BottomEdge > B_TopEdge) && (A_TopEdge < B_BottomEdge));
	}

	bool AreCollidingWithRotation(Vector4 ObjectA, Vector4 ObjectB, Vector2 projectionVector)
	{
		float A_TopEdge = ObjectA.x;
		float A_RightEdge = ObjectA.y;
		float A_BottomEdge = ObjectA.z;
		float A_LeftEdge = ObjectA.w;

		float B_TopEdge = ObjectB.x;
		float B_RightEdge = ObjectB.y;
		float B_BottomEdge = ObjectB.z;
		float B_LeftEdge = ObjectB.w;

		return ((A_LeftEdge < B_RightEdge) && (A_RightEdge > B_LeftEdge) && (A_BottomEdge > B_TopEdge) && (A_TopEdge < B_BottomEdge));
	}

	Vector2 Lerp(Vector2 startPos, Vector2 endPos, float ease)
	{
		Vector2 difference = Vector2(endPos.x - startPos.x, endPos.y - startPos.y);
		Vector2 easedDiff = Vector2(difference.x * ease, difference.y * ease);
		return Vector2(startPos.x + easedDiff.x, startPos.y + easedDiff.y);
	}

	std::vector<std::shared_ptr<GameObject>> RayCast(Vector2 origin, Vector2 direction, float distance)
	{
		Vector2 correctedOrigin = Vector2(FlatGui::sceneViewCenter.x + origin.x * FlatGui::sceneViewGridStep.x, FlatGui::sceneViewCenter.y - origin.y * FlatGui::sceneViewGridStep.y);
		float hypotenuse = sqrt(direction.x * direction.x + direction.y * direction.y);
		float divisor = hypotenuse / distance;
		Vector2 endPoint = Vector2(correctedOrigin.x + direction.x / divisor * FlatGui::sceneViewGridStep.x, correctedOrigin.y - direction.y / divisor * FlatGui::sceneViewGridStep.y);

		//ImGui::Begin("Scene View");
		//ImDrawList* drawList = ImGui::GetWindowDrawList();
		//DrawLine(correctedOrigin, endPoint, FlatGui::whiteColor, 3, drawList);
		//ImGui::End();
		return std::vector<std::shared_ptr<GameObject>>();
	}

	std::vector<std::shared_ptr<GameObject>> SphereCast(Vector2 origin, Vector2 direction, float distance)
	{
		return std::vector<std::shared_ptr<GameObject>>();
	}
}