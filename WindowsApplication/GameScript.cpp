#include "GameScript.h"



namespace FlatEngine
{
	GameScript::GameScript()
	{
		this->entities = std::vector<std::shared_ptr<GameObject>>();
		this->_isActive = true;
	}

	GameScript::~GameScript()
	{

	}	

	void GameScript::AddEntity(std::shared_ptr<GameObject> entity)
	{
		this->entities.push_back(entity);
	}

	std::vector<std::shared_ptr<GameObject>> GameScript::GetEntities()
	{
		return this->entities;
	}
}