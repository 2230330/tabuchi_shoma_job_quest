#pragma once
#include"i_component.h"

struct ComponentCloudDome :public IComponent
{
    int iteration{ 128 };//â_Ç™ë∂ç›Ç∑ÇÈå¿äEì_
	DirectX::XMFLOAT2 wind_direction = { 1.0f, 0.0f };
	DirectX::XMFLOAT2 cloud_altitudes_min_max = { 6373500.0f, 6375000.0f }; // highest and lowest altitudes at which clouds are distributed

	float wind_speed = 1.0f; // [0.0, 20.0]

	float density_scale = 0.05f; // [0.01, 0.2]
	float cloud_coverage_scale = 0.2f; // [0.1, 1.0]
	float rain_cloud_absorption_scale = 0.5;
	float cloud_type_scale = 1.0f;

	float earth_radius = 6370000.0f; // earth radius
	float horizon_distance_scale = 1.0f;
	float low_frequency_perlin_worley_sampling_scale = 0.000021f;
	float high_frequency_worley_sampling_scale = 0.0003f;
	float cloud_density_long_distance_scale = 18.0f;
	int enable_powdered_sugar_efffect = false;

	int ray_marching_steps = 128;
	int auto_ray_marching_steps = false;
};