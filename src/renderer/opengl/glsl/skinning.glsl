//   _____ ______             _
//  / ____|  ____|           (_)
// | |    | |__   _ __   __ _ _ _ __   ___
// | |    |  __| | '_ \ / _` | | '_ \ / _ |
// | |____| |____| | | | (_| | | | | |  __/
//  \_____|______|_| |_|\__, |_|_| |_|\___|
//                       __/ |
//                      |___/

uniform samplerBuffer joint_palette;
uniform int joint_count;

mat4 cengine_joint_matrix(uint joint)
{
    int first = int(joint) * 4;
    return mat4(
        texelFetch(joint_palette, first + 0),
        texelFetch(joint_palette, first + 1),
        texelFetch(joint_palette, first + 2),
        texelFetch(joint_palette, first + 3));
}

mat4 cengine_skin_matrix(uvec4 joints, vec4 weights)
{
    if (joint_count <= 0)
    {
        return mat4(1.0);
    }
    mat4 result = mat4(0.0);
    for (int influence = 0; influence < 4; ++influence)
    {
        if (weights[influence] > 0.0 && int(joints[influence]) < joint_count)
        {
            result += cengine_joint_matrix(joints[influence]) * weights[influence];
        }
    }
    return result;
}
