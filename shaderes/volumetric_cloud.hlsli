
cbuffer CLOUD_RAY_MARCHING_CONSTNAT_BUFFER : register(b12)
{
    float2 wind_direction;
    float2 cloud_altitudes_min_max; // highest and lowest altitudes at which clouds are distributed
	
    float wind_speed; 
	
    float density_scale; 
    float cloud_coverage_scale; // [0.1, 1.0]
    float rain_cloud_absorption_scale;
    float cloud_type_scale;

    float earth_radius; // earth radius
    float horizon_distance_scale;
    float low_frequency_perlin_worley_sampling_scale;
    float high_frequency_worley_sampling_scale;
    float cloud_density_long_distance_scale;
    bool enable_powdered_sugar_efffect;
	
    uint ray_marching_steps;
    bool auto_ray_marching_steps;

    float2 object_resolution; //オブジェクトの解像度
};