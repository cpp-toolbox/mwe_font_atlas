#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "font_atlas/font_atlas.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "texture_atlas/texture_atlas.hpp"
#include "window/window.hpp"
#include "shader_cache/shader_cache.hpp"

#include <cstdio>
#include <cstdlib>

unsigned int SCREEN_WIDTH = 800;
unsigned int SCREEN_HEIGHT = 800;

static void error_callback(int error, const char *description) { fprintf(stderr, "Error: %s\n", description); }

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

struct OpenGLDrawingData {
    GLuint vbo_name;
    GLuint ibo_name;
    GLuint vao_name;
};

OpenGLDrawingData prepare_drawing_data_and_opengl_drawing_data(ShaderCache &shader_cache, FontAtlas &font_atlas,
                                                               const std::string &text) {
    TextMesh text_mesh = font_atlas.generate_text_mesh_with_width(text, -1, 0, 1, 0.1);

    // vbo: vertex buffer object
    // vao: vertex array object
    // ibo: index buffer object

    GLuint vbo_name, tcbo_name, vao_name, ibo_name;

    glGenVertexArrays(1, &vao_name);
    glGenBuffers(1, &vbo_name);
    glGenBuffers(1, &tcbo_name);
    glGenBuffers(1, &ibo_name);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and
    // then configure vertex attributes(s).
    glBindVertexArray(vao_name);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_name);
    glBufferData(GL_ARRAY_BUFFER, text_mesh.vertex_positions.size() * sizeof(glm::vec3),
                 text_mesh.vertex_positions.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, tcbo_name);
    glBufferData(GL_ARRAY_BUFFER, text_mesh.texture_coordinates.size() * sizeof(glm::vec2),
                 text_mesh.texture_coordinates.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_name);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, text_mesh.indices.size() * sizeof(unsigned int), text_mesh.indices.data(),
                 GL_STATIC_DRAW);

    shader_cache.configure_vertex_attributes_for_drawables_vao(vao_name, vbo_name,
                                                               ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT,
                                                               ShaderVertexAttributeVariable::POSITION);

    shader_cache.configure_vertex_attributes_for_drawables_vao(
        vao_name, tcbo_name, ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT,
        ShaderVertexAttributeVariable::PASSTHROUGH_TEXTURE_COORDINATE);

    // note that this is allowed, the call to glVertexAttribPointer registered
    // vbo_name as the vertex attribute's bound vertex buffer object so afterwards
    // we can safely unbind
    /*glBindBuffer(GL_ARRAY_BUFFER, 0);*/

    return {vbo_name, ibo_name, vao_name};
}

int main() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::debug);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("mwe_shader_cache_logs.txt", true);
    file_sink->set_level(spdlog::level::info);

    std::vector<spdlog::sink_ptr> sinks = {console_sink, file_sink};

    LiveInputState live_input_state;

    GLFWwindow *window = initialize_glfw_glad_and_return_window(&SCREEN_WIDTH, &SCREEN_HEIGHT, "mwe font atlas", true,
                                                                false, false, &live_input_state);

    std::vector<ShaderType> requested_shaders = {ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT};
    ShaderCache shader_cache(requested_shaders, sinks);
    FontAtlas font_atlas("assets/times_64_sdf_atlas_font_info.json", "assets/times_64_sdf_atlas.json",
                         "assets/times_64_sdf_atlas.png", SCREEN_WIDTH, false, true);

    std::string text = "text rendering with SDFs!";
    auto text_color = glm::vec3(0.5, 0.5, 1);
    float char_width = 0.5;
    float edge_transition = 0.1;

    auto [vbo_name, ibo_name, vao_name] = prepare_drawing_data_and_opengl_drawing_data(shader_cache, font_atlas, text);

    int width, height;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    while (!glfwWindowShouldClose(window)) {

        glfwGetFramebufferSize(window, &width, &height);

        glViewport(0, 0, width, height);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::mat4(1);
        /*glm::ortho(0.0f, static_cast<float>(SCREEN_WIDTH), 0.0f, static_cast<float>(SCREEN_HEIGHT));*/

        shader_cache.use_shader_program(ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT);
        shader_cache.set_uniform(ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT,
                                 ShaderUniformVariable::TRANSFORM, projection);

        shader_cache.set_uniform(ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT,
                                 ShaderUniformVariable::RGB_COLOR, text_color);

        shader_cache.set_uniform(ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT,
                                 ShaderUniformVariable::CHARACTER_WIDTH, char_width);

        shader_cache.set_uniform(ShaderType::TRANSFORM_V_WITH_SIGNED_DISTANCE_FIELD_TEXT,
                                 ShaderUniformVariable::EDGE_TRANSITION_WIDTH, edge_transition);

        glBindVertexArray(vao_name); // seeing as we only have a single VAO there's
                                     // no need to bind it every time, but we'll do
                                     // so to keep things a bit more organized
        glDrawElements(GL_TRIANGLES, 6 * text.size(), GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);

    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &vao_name);
    glDeleteBuffers(1, &vbo_name);
    glDeleteBuffers(1, &ibo_name);
    /*glDeleteProgram(shader_program);*/

    glfwTerminate();
    exit(EXIT_SUCCESS);
}
