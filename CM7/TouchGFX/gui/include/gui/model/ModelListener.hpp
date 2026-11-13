#ifndef MODELLISTENER_HPP
#define MODELLISTENER_HPP

#include <gui/model/Model.hpp>

class ModelListener
{
public:
    ModelListener() : model(0) {}

    virtual ~ModelListener() {}

    void bind(Model* m)
    {
        model = m;
    }

    virtual void gaugeStartAnimationIsDone() {};
    virtual void speedUpdated(float speed) {};

    /* Callbacks CAN data — appelés par Model::tick() à chaque trame reçue */
    virtual void temperatureUpdated(float temp) {};
    virtual void fuelUpdated(float fuel) {};
    virtual void lidarUpdated(float lidar_cm) {};

    /* Appelé par Model::tick() quand l'état audio a changé */
    virtual void audioInfoUpdated() {};

protected:
    Model* model;
};

#endif // MODELLISTENER_HPP
