if (ORYOL_OPENGL)

fips_begin_app(TestNanoVG windowed)
    fips_files(TestNanoVG.cc demo.cc demo.h)
    fips_deps(Core IO HTTP Gfx Input Time NanoVG)
    oryol_add_web_sample(TestNanoVG "NanoVG integration" "emscripten,pnacl,android" TestNanoVG.jpg "TestNanoVG/TestNanoVG.cc")
fips_end_app()

endif()