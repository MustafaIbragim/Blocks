// Проверка: барьеры на краю чанков должны останавливать динамику.
#include <cstdio>
#include <memory>

#include "block.h"
#include "chunk.h"
#include "texture.h"
#include "world.h"
#include "physics_world.h"

int main() {
    BlockManager block_mgr;
    TextureBlocksManager tex_blocks_mgr;
    TextureManager tex_mgr;
    World world(block_mgr, tex_blocks_mgr, tex_mgr);

    // Один загруженный чанк (0,0) с полом на y=10.
    world.set_chunk(Vec2i(0, 0), std::make_shared<Chunk>(Vec2i(0, 0)));

    auto dirt = std::make_shared<Block>(0, "dirt");
    dirt->opacity = 255;
    uint16_t dirt_id = block_mgr.register_block(dirt);
    (void)dirt_id;
    for (int x = 0; x < 32; ++x) {
        for (int z = 0; z < 32; ++z) {
            world.set_block(Vec3i(x, 10, z), dirt);
        }
    }

    PhysicsWorld physics;
    physics.add_chunk(world, Vec2i(0, 0));

    int failures = 0;

    // 1. get_block за пределами загруженных чанков — барьер с коллизией.
    auto outside = world.get_block(Vec3i(40, 20, 16));
    if (!outside || !outside->has_collision()) {
        printf("FAIL: get_block outside loaded chunk has no collision\n");
        ++failures;
    } else {
        printf("OK: get_block outside returns barrier with collision (name=%s)\n",
               outside->name.c_str());
    }

    // 2. Сфера летит горизонтально к краю чанка (x=32). Барьер незагруженного
    //    чанка (1,0) должен её остановить.
    uint32_t ball = physics.add_dynamic_sphere(0.4f, 1.0f, Vec3f(30.0f, 15.5f, 16.0f));
    physics.set_body_gravity(ball, Vec3f(0.0f, 0.0f, 0.0f));  // без падения
    physics.set_body_linear_velocity(ball, Vec3f(20.0f, 0.0f, 0.0f));
    for (int i = 0; i < 600; ++i) {
        physics.step(1.0f / 60.0f, 1, 1.0f / 60.0f);
    }
    Vec3f pos = physics.get_body_position(ball);
    printf("ball position after flight to +X edge: %.3f %.3f %.3f\n", pos.x, pos.y, pos.z);
    if (pos.x > 32.0f - 0.4f + 0.01f) {
        printf("FAIL: sphere crossed the chunk-edge barrier (x=%.3f)\n", pos.x);
        ++failures;
    } else {
        printf("OK: sphere stopped by barrier at chunk edge\n");
    }

    // 3. Контроль: внутри чанка над полом сфера покоится (не проваливается).
    uint32_t rest = physics.add_dynamic_sphere(0.5f, 1.0f, Vec3f(16.0f, 20.0f, 16.0f));
    for (int i = 0; i < 360; ++i) {
        physics.step(1.0f / 60.0f, 1, 1.0f / 60.0f);
    }
    Vec3f rest_pos = physics.get_body_position(rest);
    printf("rest sphere position: %.3f %.3f %.3f\n", rest_pos.x, rest_pos.y, rest_pos.z);
    if (!(rest_pos.y > 11.2f && rest_pos.y < 11.9f)) {
        printf("FAIL: control sphere did not rest on floor\n");
        ++failures;
    } else {
        printf("OK: control sphere rests on floor\n");
    }

    if (failures == 0) {
        printf("BARRIER TEST: OK\n");
        return 0;
    }
    printf("BARRIER TEST: %d FAILURES\n", failures);
    return 1;
}
