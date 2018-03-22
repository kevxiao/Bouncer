#include "JointNode.hpp"

//---------------------------------------------------------------------------------------
JointNode::JointNode(const std::string& name)
	: SceneNode(name)
{
	m_nodeType = NodeType::JointNode;
}

//---------------------------------------------------------------------------------------
JointNode::~JointNode() {

}
 //---------------------------------------------------------------------------------------
void JointNode::set_joint_x(double min, double init, double max) {
	m_joint_x.min = min;
	m_joint_x.init = init;
	m_joint_x.max = max;
    m_current_x = init;
}

//---------------------------------------------------------------------------------------
void JointNode::set_joint_y(double min, double init, double max) {
	m_joint_y.min = min;
	m_joint_y.init = init;
	m_joint_y.max = max;
    m_current_y = init;
}

double JointNode::rotate_y(double angle) {
    m_current_y += angle;
    if (m_current_y > m_joint_y.max) {
        angle = angle - (m_current_y - m_joint_y.max);
        m_current_y = m_joint_y.max;
    }
    if (m_current_y < m_joint_y.min) {
        angle = angle - (m_current_y - m_joint_y.min);
        m_current_y = m_joint_y.min;
    }
    return angle;
}

double JointNode::rotate_x(double angle) {
    m_current_x += angle;
    if (m_current_x > m_joint_x.max) {
        angle = angle - (m_current_x - m_joint_x.max);
        m_current_x = m_joint_x.max;
    }
    if (m_current_x < m_joint_x.min) {
        angle = angle - (m_current_x - m_joint_x.min);
        m_current_x = m_joint_x.min;
    }
    return angle;
}

void JointNode::reset_rotation() {
    m_current_x = m_joint_x.init;
    m_current_y = m_joint_y.init;
}