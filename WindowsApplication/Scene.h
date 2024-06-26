#pragma once
#include <vector>
#include <string>
#include <memory>
#include <list>

namespace FlatEngine
{
	class GameObject;
	class Camera;

	class Scene
	{
		friend class SceneManager;
	public:
		Scene();
		~Scene();

		void SetName(std::string name);
		std::string GetName();
		void SetPath(std::string path);
		std::string GetPath();
		void AddSceneObject(std::shared_ptr<GameObject> sceneObject);
		std::vector<std::shared_ptr<GameObject>> GetSceneObjects();
		void SetAnimatorPreviewObjects(std::vector<std::shared_ptr<GameObject>> previewObjects);
		std::vector<std::shared_ptr<GameObject>> &GetAnimatorPreviewObjects();
		std::shared_ptr<GameObject> GetObjectById(long ID);
		std::shared_ptr<GameObject> GetObjectByName(std::string name);
		std::shared_ptr<GameObject> CreateGameObject(long parentID = -1);
		void DeleteGameObject(long sceneObjectID);
		void DeleteChildrenAndSelf(std::shared_ptr<GameObject> objectToDelete); // Recursive
		void IncrementGameObjectID();
		long GetNextGameObjectID();
		void IncrementComponentID();
		long GetNextComponentID();
		void SetPrimaryCamera(std::shared_ptr<Camera> camera);
		void RemovePrimaryCamera();
		std::shared_ptr<Camera> GetPrimaryCamera();

	private:
		std::string name;
		std::string path;
		std::vector<std::shared_ptr<GameObject>> sceneObjects;	
		std::vector<std::shared_ptr<GameObject>> animatorPreviewObjects;
		std::shared_ptr<Camera> primaryCamera;
		long nextGameObjectID;
		long nextComponentID;
	};
}