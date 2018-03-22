#pragma once

#include "SceneNode.hpp"

class JointNode : public SceneNode {
public:
	JointNode(const std::string & name);
	virtual ~JointNode();

	void set_joint_x(double min, double init, double max);
	void set_joint_y(double min, double init, double max);
    double rotate_y(double angle);
    double rotate_x(double angle);
    void reset_rotation();

	struct JointRange {
		double min, init, max;
	};


	JointRange m_joint_x, m_joint_y;
    double m_current_x, m_current_y;
};
