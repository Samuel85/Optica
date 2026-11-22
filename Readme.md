# Óptica

*Óptica*, or for simplicity and to avoid using Latin characters, *Optica*, is a tool for teaching geometrical optics. Based on the previous work [OptiC](https://github.com/Samuel85/OptiC), this project improves its design and fixes previous bugs, adding new features like fullscreen mode and virtual and real images with labels. Written in C++20 and using AI, this project was more of a challenge: to explore AI's ability to revive an almost 20-year-old project and bring it to new platforms.

<center>
<img src="./screenshots/windows.png" width=50%>
</center>

# Changes
* Replaces old code for linked lists with modern C++ to avoid memory leaks.
* Rewrites the ray tracing algorithm.
* Decouples the old monolithic design with the Model-View-Controller (MVC) pattern.
* New _Linux_, _Android_ and _iOS_ ports using _SDL3_ and Dear _ImGUI_ (See the [screenshot directory](./screenshots/)).

# Building
Optica builds with CMake on Windows, Linux, Android and iOS. [BUILDING.md](./BUILDING.md)
describes how to install the dependencies and compile it for each platform, and how the
GitHub Actions builds publish ready-made packages with each release.
