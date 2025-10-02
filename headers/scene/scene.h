#ifndef PART2_SCENE_H
#define PART2_SCENE_H

class Scene
{
public:
    Scene() = default;
    virtual ~Scene() = default;
    //XVˆ—
    virtual void Update(float elapsed_time) {};
    //•`‰æˆ—
    virtual void Render(float elapsed_time) {};
    //GUI•`‰æˆ—
    virtual void DrawGui() {};
};
#endif // !PART2_SCENE_H
