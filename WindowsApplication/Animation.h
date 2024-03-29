#pragma once
#include "Component.h"



namespace FlatEngine
{
	class Animation : public Component
	{
	public:
		Animation(long myID = -1, long parentID = -1);
		~Animation();

		void AddFrame();
		void Play();
		void Stop();

		bool IsPlaying();
		void SetTicksPerFrame(float ticksPerFrame);
		float GetTicksPerFrame();
		std::string GetData();

		// Test Animations
		void LerpToCenter();

	private:
		bool _playing;
		float ticksPerFrame;
		int animationStartTime;
	};
}

