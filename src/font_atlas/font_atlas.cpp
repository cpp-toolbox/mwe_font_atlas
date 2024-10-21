#include "font_atlas.hpp"
#include <iostream>
#include <fstream>

using json = nlohmann::json;

FontAtlas::FontAtlas(const std::string &font_info_json_filepath, const std::string &texture_atlas_json_filepath,
                     const std::string &texture_filepath, unsigned int screen_width_px, bool flip_texture,
                     bool top_left_coords)
    : texture_atlas(texture_atlas_json_filepath, texture_filepath, flip_texture, top_left_coords) {
    // load the font metadata from the json file
    std::ifstream file(font_info_json_filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open font atlas file: " << font_info_json_filepath << std::endl;
        return;
    }

    float average_char_width_px = 0;

    json j;
    file >> j;

    /*name = j["name"];*/
    /*size = j["size"];*/
    /*bold = j["bold"];*/
    /*italic = j["italic"];*/
    /*atlas_width = j["width"];*/
    /*atlas_height = j["height"];*/

    auto json_characters = j["characters"];
    for (auto &[charKey, charData] : json_characters.items()) {
        Character character;
        character.width_px = charData["width"];
        character.height_px = charData["height"];
        character.origin_x = charData["originX"];
        character.origin_y = charData["originY"];
        character.x_dist_to_next_char_px = charData["advance"];

        average_char_width_px += character.width_px;

        // get the uv coordinates for each character from the texture atlas
        std::string sprite_name(1, charKey[0]); // convert char to string
        character.uv_coordinates = texture_atlas.get_texture_coordinates_of_sub_texture(sprite_name);

        characters[charKey[0]] = character;
    }
    average_char_width_px /= json_characters.size();

    // In NDC the screen's length has [-1, 1], which has length 2
    // so let L = 2 to represent this, then suppose we want to fit
    // 50 chars into this screen by default, and we set NC = 50

    // the characters widths are defined in terms of pixel count so first we need to
    // convert them so that at least they fit into NDC roughly, so we compute the
    // average width of all the chars (ACW) and divide them all by this value
    // forcing this value to be approximately within [0, 1] which means that
    // any char would fit into the right half of the screen thus to fit
    // into the entire frame of the screen we would do use a scale of 2 / ACW

    // now given a character's width CW we do CW * (2 / ACW) which makes it fit NDC space bounds
    // then we want it so that we can fit 50 of them in so we divide by NC

    default_scale = ((2 / average_char_width_px)) * 1 / num_chars_per_screen_width;
}

FontAtlas::~FontAtlas() {
    // cleanup if necessary (textureatlas handles opengl texture cleanup)
}

TextMesh FontAtlas::generate_text_mesh(const std::string &text, float x, float y, float scale) {
    TextMesh mesh;
    std::vector<std::vector<unsigned int>> index_batches; // List of lists of indices for each quad
    unsigned int index_offset = 0;

    scale = default_scale * scale;

    for (const char &c : text) {
        if (characters.find(c) == characters.end()) {
            std::cerr << "Character '" << c << "' not found in the font atlas." << std::endl;
            continue;
        }

        Character &ch = characters[c];

        float xpos = x - ch.origin_x * scale;
        float ypos = y - (ch.height_px - ch.origin_y) * scale;
        /*float ypos = y - ch.originY * scale;*/
        float w = ch.width_px * scale;
        float h = ch.height_px * scale;

        std::vector<glm::vec3> char_vertices = generate_rectangle_vertices(xpos + w / 2, ypos + h / 2, w, h);

        mesh.vertex_positions.insert(mesh.vertex_positions.end(), char_vertices.begin(), char_vertices.end());

        // Use the stored UV coordinates for this character
        mesh.texture_coordinates.insert(mesh.texture_coordinates.end(), ch.uv_coordinates.begin(),
                                        ch.uv_coordinates.end());

        std::vector<unsigned int> char_indices = generate_rectangle_indices();
        index_batches.push_back(char_indices);

        x += (ch.x_dist_to_next_char_px * scale);
    }

    mesh.indices = flatten_and_increment_indices(index_batches);

    return mesh;
}
