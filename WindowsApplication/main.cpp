#pragma once
#include <SDL.h>
#include "Window.h"
#include "FlatEngine.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include <SDL_mixer.h>

//Main loop flag
bool _hasQuit = false;

bool init()
{
	//Initialization flag
	bool success = true;

	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	{
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		success = false;
	}
	else
	{
		FlatEngine::logger->LogString("SDL initialized...");

		//Set texture filtering to linear
		if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"))
		{
			printf("Warning: Linear texture filtering not enabled!");
		}

		char t[] = "Window Title";
		char* title = &(t[0]);

		//Initialize Window::window
		if (Window::Init(title, 600, 400))
		{
			FlatEngine::LogString("Window initialized...");

			//Initialize SDL_image for png loading
			int imgFlags = IMG_INIT_PNG;
			if (!(IMG_Init(imgFlags) & imgFlags))
			{
				printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
				success = false;
			}
			else
			{
				FlatEngine::LogString("SDL_image initialized...");
				//Initialize SDL_ttf for text rendering
				if (TTF_Init() == -1)
				{
					printf("SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError());
					success = false;
				}
				else
				{
					//Initialize SDL_mixer
					if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
					{
						printf("SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError());
						success = false;
					}
					else
					{
						FlatEngine::LogString("SDL_mixer initialized...");
						FlatEngine::LogString("Ready...");
						//Initialize next thing
					}
				}
			}
		}
	}

	return success;
}


bool loadMedia()
{
	return TextureManager::LoadTextures();
};


void close()
{
	//Quit TextureManager
	TextureManager::Cleanup();

	//Quit ImGui
	FlatEngine::FlatGui::Cleanup();

	//Quit SDL subsystems
	Mix_Quit();
	TTF_Quit();
	IMG_Quit();
	SDL_Quit();
}


int main(int argc, char* args[])
{
	//Start up SDL and create window
	if (!init())
	{
		printf("Failed to initialize!\n");
	}
	else
	{
		FlatEngine::FlatGui::SetupImGui();

		//Load media
		if (!loadMedia())
		{
			printf("Failed to load media!\n");
		}
		else
		{
			//While application is running
			while (!_hasQuit)
			{
				FlatEngine::Run(_hasQuit);
			}
		}
	}

	close();

	return 0;
}