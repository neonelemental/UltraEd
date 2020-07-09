#ifndef _SPHERECOLLIDER_H_
#define _SPHERECOLLIDER_H_

#include "Collider.h"

using namespace std;

namespace UltraEd
{
    class SphereCollider : public Collider
    {
    public:
        SphereCollider();
        SphereCollider(const vector<Vertex> &vertices);
        void Build();
        FLOAT GetRadius() { return m_radius; }
        cJSON *Save();
        bool Load(cJSON *root);

    private:
        void SphereFromDistPoints(D3DXVECTOR3 &center, FLOAT &radius, const vector<Vertex> &vertices);
        void AdjustSphere(D3DXVECTOR3 &center, FLOAT &radius, const Vertex &vertex);
        void FindCenterWithRadius(D3DXVECTOR3 &center, FLOAT &radius, const vector<Vertex> &vertices);

    private:
        FLOAT m_radius;
    };
}

#endif
