#pragma once
#include "Component.h"
#include "Texture.h"
#include <SDL.h>
#include "json.hpp"
using json = nlohmann::json;
using namespace nlohmann::literals;


namespace FlatEngine
{
	class Sprite : public Component
	{
	public:
		Sprite(long myID = -1, long parentID = -1);
		Sprite(std::shared_ptr<Sprite> toCopy, long newParentID);
		~Sprite();
		void SetTexture(std::string path);
		SDL_Texture* GetTexture();
		void SetScale(Vector2 newScale);
		Vector2 GetScale();
		float GetTextureWidth();
		float GetTextureHeight();
		std::string GetPath();
		void SetRenderOrder(int order);
		int GetRenderOrder();
		void SetOffset(Vector2 offset);
		Vector2 GetOffset();
		void RemoveTexture();
		std::string GetData();

	private:
		SDL_Texture* texture;
		int renderOrder;
		float textureWidth;
		float textureHeight;
		Vector2 scale;
		Vector2 offset;
		std::string path;
	};
}
