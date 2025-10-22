<div align="center">

# Asmadi Engine

A Simple, ECS-based game engine written in C using SDL3's GPU API

</div>

## Overview

The Asmadi Game Engine is a game engine using SDL3's GPU API to create 3D accelerated graphical applications with Vulkan (and soon hopefully WebGPU). I might add Metal support if I feel like it.

### Goals

The main goal with this project was to learn graphics programming. In game development, I often find myself knowing exactly what I want the code to look like, but unsure of how to tell the game engine how to do that. So, I figured that several hundred to several thousand hours of trial and error should save me about thirty minutes of reading the Godot Documentation.

### Features

- [ ] Three.js-like API for creating simple geometries
    - [X] ~~Box~~
    - [X] ~~Plane~~
    - [X] ~~Capsule~~
    - [X] ~~Circle~~
    - [X] ~~Cone~~
    - [X] ~~Cylinder~~
    - [X] ~~Dodecahedron~~
    - [ ] Extrude
    - [X] ~~Icosahedron~~
    - [X] ~~Lathe~~
    - [X] ~~Octahedron~~
    - [X] ~~Ring~~
    - [ ] Shape
    - [X] ~~Sphere~~
    - [ ] Ico Sphere
    - [ ] That one other sphere... you know the one
    - [X] ~~Tetrahedron~~
    - [X] ~~Torus~~
    - [ ] Torus Knot?
    - [ ] Tube
    - [ ] GLTF loader
- [ ] Various Math Tools
    - [X] ~~Random Integers~~
    - [X] ~~Random Floats~~
    - [X] ~~Random Choice~~
    - [ ] Weighted Randomness
    - [ ] Splines
    - [ ] LERP
- [ ] Materials
    - [X] ~~Basic Material~~
    - [ ] Depth Material
    - [ ] Distance Material?
    - [ ] Lambert Material
    - [ ] Matcap Material?
    - [ ] Normal Material
    - [X] ~~Phong Material~~
    - [ ] Standard/Physical Material
    - [ ] Toon Material
    - [ ] UV Mapping for Mesh Primitives
- [ ] Character Controllers
- [ ] Physics
    - [ ] TinyPhysicsEngine-like soft body physics for embedded devices
    - [ ] SDF-based primitive-only collision system
    - [ ] Conventional primitive and mesh collision system with soft body support
- [ ] Lighting
    - [ ] Directional Ambient Lights
- [ ] Performance Profiler
- [ ] Software Rasterizer
- [ ] Fixed-Point Software Rasterizer