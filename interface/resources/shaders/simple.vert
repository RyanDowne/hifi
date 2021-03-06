#version 120

//
//  simple.vert
//  vertex shader
//
//  Created by Andrzej Kapolka on 9/15/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

// the interpolated normal
varying vec4 normal;

void main(void) {
    // transform and store the normal for interpolation
    normal = normalize(gl_ModelViewMatrix * vec4(gl_Normal, 0.0));
    
    // pass along the diffuse color
    gl_FrontColor = gl_Color;
    
    // use standard pipeline transform
    gl_Position = ftransform();
}
