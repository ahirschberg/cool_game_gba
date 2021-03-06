#include "../myLib.h"
#include "../random.h"
#include "../main.h"
#include "../entities.h"
#include "game_state.h"
#include "game.h"
#include "difficulty.h"
#include "difficulty_shooter.h"

#include "../assets/tile_scroller.h"
#include "../assets/game_over.h"
#include "../render/game_title.h"

uint32_t score;
bool game_ee_mode;
bool ee_longTitle;
uint8_t hud_mode;

void reset() {
    score = 0;
    game_ee_mode = FALSE;
    ee_longTitle = FALSE;
    hud_mode = 0;
    Difficulty_reset();
}

// game over
uint8_t game_over_anim_frame = 0;

void drawFloor(uint8_t offset) {
    drawImageFW(GROUND_OFFSET, 16, 16 * offset, tile_scroller);
}

enum GAME_STATE paused_lastState = START_SCREEN;
void setPaused(bool isPaused) {
    if (isPaused) {
        paused_lastState = gameState;
    } else {
        gameState = paused_lastState;
    }
}

void initState(const enum GAME_STATE state) {
    PLAYER_ENTITY->f_invuln = 0; // prevent weird things from happening between states
    Difficulty_init(state, score);

    if ((state == RUNNER_TRANSITION || state == SHOOTER_TRANSITION) && gameState == START_SCREEN_NODRAW) {
        GameTitle_remove_from_background();
    }

    switch(state) {
        case START_SCREEN:
            reset();
            clearEntities(0);
            addEntity(PLAYER_STAND_TID, 110 + 3, GROUND_OFFSET, PLAYER);
            redrawBG2(0, SCREEN_HEIGHT);
            GameTitle_add_to_background();
            drawFloor(0);
            break;
        case START_SCREEN_NODRAW:
            break;
        case RUNNER_TRANSITION:
            clearEntities(1);
            break;
        case RUNNER:
            clearEntities(1);

            hud_mode = 1; redrawHUD();

            setRunning(PLAYER_ENTITY, 0);
            setFacing(PLAYER_ENTITY->obj, 1);

            PLAYER_ENTITY->dx = ((int8_t) -1) * runner_conf->scroller_dx;
            break;
        case SHOOTER_TRANSITION:
            clearEntities(1);
            hud_mode = 2; redrawHUD();
            PLAYER_ENTITY->health = 5;
            setWalking(PLAYER_ENTITY, DIR_RIGHT);
            break;
        case SHOOTER:
            PLAYER_ENTITY->state=STANDING;
            PLAYER_ENTITY->dx = 0;
            ENEMY_DATA->ticks_until_next_spawn = 10;
            break;
        case GAME_OVER:
            hud_mode = 0;
            PLAYER_ENTITY->state = STANDING;
            drawRectFW(0, SCREEN_HEIGHT, BYTETOWORD(WHITE));
            break;
        case GAME_OVER_NODRAW:
            ;
            const int sdraw = 20;
            drawString(75, sdraw, "Press START!", BLACK);
            drawString(75 + 20, sdraw, "Programmed by Alex Hirschberg", BLACK);
            drawString(75 + 32, sdraw, "Art by Chris Thompson", BLACK);
            drawString(75 - 44, sdraw, "Fork me on GitHub! bit.ly/hmgba", RED); // 1 char longer and not custom: goo.gl/ptBHwM
            PLAYER_ENTITY->dx = 0;
            break;
        default:
            break;
    }
    gameState = state;
}

void gameOver() {
    if (!game_ee_mode) initState(GAME_OVER);
    else PUTS("Prevented game from ending.");
}

int rnum = 0x0;
uint8_t last_player_health = -1;

uint8_t which_rock_tid = 0;
void tickGame(const uint32_t frame) {
    const uint8_t frame4s = frame & 0xFF;
    switch(gameState) {
        case START_SCREEN_NODRAW:
            if (ee_longTitle && frame4s == rnum) {
                const int action_sel = qran_range(0, 13);
                int movement_dir = qran_range(0, 2);
                if (movement_dir == 0) movement_dir = -1;
                if (action_sel < 4) setWalking(PLAYER_ENTITY, movement_dir);
                else if (action_sel < 10) setRunning(PLAYER_ENTITY, movement_dir);
                else setJumping(PLAYER_ENTITY, 20);

                if (frame > 60 * 60) {
                    BF_SET(PLAYER_ENTITY->obj->attr2, 4, ATTR2_PALBANK);
                }

                if (qran_range(0, 4) == 0) {
                    setJumping(PLAYER_ENTITY, 10);
                }
                rnum = qran_range(0, 0xF);
            } else if (!ee_longTitle && frame4s == rnum) {
                setJumping(PLAYER_ENTITY, frame / (60 * 5) + 5);
                if (frame > 60 * 30) {
                    ee_longTitle = TRUE;
                }
            }
            break;
        case RUNNER_TRANSITION:
            ;
            const int8_t dir = TRIBOOL(10 - ENT_X(PLAYER_ENTITY));
            setRunning(PLAYER_ENTITY, dir); // player walks towards col 10

            if (IABS(ENT_X(PLAYER_ENTITY) - 10) < 4) {
                BF_SET(PLAYER_ENTITY->obj->attr1, 10, ATTR1_X);
                rnum = 100; // FIXME hack to slow down the obstacles
                initState(RUNNER);
            }
            break;
        case RUNNER:
            if (Difficulty_shouldTransition(score)) {
                Difficulty_next(score);
                initState(SHOOTER_TRANSITION);
            } else {
                if ((frame & 0xF) == 0) {
                    drawFloor(0);
                } else if ((frame & 0xF) == 8) {
                    drawFloor(1);
                }
                if (--rnum <= 0) {
                    if (objs_length < runner_conf->max_objs) {
                        const int ran = qran_range(0, 3);
                        switch(ran) {
                            case 0:
                                ;
                                const uint32_t rock_tid = (which_rock_tid++ & 1) ? OBSTACLE_ROCK1_TID : OBSTACLE_ROCK2_TID;
                                addEntity(rock_tid, SCREEN_WIDTH, GROUND_OFFSET, OBSTACLE_ROCK);
                                break;
                            case 1:
                                addEntity(OBSTACLE_CACTUS_TID, SCREEN_WIDTH, GROUND_OFFSET, OBSTACLE_CACTUS);
                                break;
                            case 2:
                                addEntity(OBSTACLE_SHEET_TID, SCREEN_WIDTH, GROUND_OFFSET, OBSTACLE_SHEET);
                                break;
                            default:
                                addEntity(PLAYER_HURT_TID, SCREEN_WIDTH, GROUND_OFFSET, OBSTACLE_ROCK);
                        }
                    }
                    rnum = qran_range(runner_conf->obstacle_spawn_delay_lower, runner_conf->obstacle_spawn_delay_upper);
                }
            }
            break;
        case SHOOTER_TRANSITION:
            if (BF_GET(PLAYER_ENTITY->obj->attr1, ATTR1_X) > SCREEN_WIDTH / 2 - 10) {
                initState(SHOOTER);
            }
            break;
        case SHOOTER:
            if (Difficulty_shouldTransition(score)) {
                Difficulty_next(score);
                initState(RUNNER_TRANSITION);
            } else {
                if (each_1_15th(frame, 0) && PLAYER_ENTITY->health != last_player_health) {
                    redrawHUD();
                }
                if (each_2_15th(frame, 4) && --ENEMY_DATA->ticks_until_next_spawn <= 0) {
                    tick_shooter_level();
                    if (ENEMY_DATA->ticks_until_next_spawn == 0) {
                        //PUTS("warn: level func not setting ticks");
                    }
                }
            }
            break;
        case GAME_OVER:
            PLAYER_ENTITY->dx = 0;
            if ((frame & 3) == 0) {
                if (game_over_anim_frame < 20) {
                    drawImageFW(50, 20, (game_over_anim_frame++) * 20, game_over);
                } else {
                    game_over_anim_frame = 0;
                    initState(GAME_OVER_NODRAW);
                }
            }
            break;
        default:
            break;
    }
    tickEntities(frame);
}

INLINE void runner_checkPlayerCollisions() {
    const uint8_t player_x = (const uint8_t) ENT_X(PLAYER_ENTITY);
    const uint8_t player_y = (const uint8_t) groundDist(PLAYER_ENTITY);
    const int player_width = attrs(PLAYER_ENTITY).width;

    int i = 1;
    while (i < objs_length) {
        const short ent_x = ENT_X(allEntities+i);

        const ENTITY_ATTRS obstacle_attrs = attrs(allEntities + i);
        const int8_t fix_sprite_height_a_little = (allEntities[i].type == OBSTACLE_SHEET) ? 0 : 4;
        if (player_y < obstacle_attrs.height - fix_sprite_height_a_little && player_x < ent_x + obstacle_attrs.width / 2 // why??
                && player_x + player_width - 5 > ent_x) {
            gameOver();
        }
        if (ent_x + obstacle_attrs.width <= 0) {
            score++;
            removeEntity(i--);
        }
        i++;
    }
}

bool hurtEntity(ENTITY* e, int8_t damage, int8_t dir) {
    setHurt(e, dir);
    if (((signed char)e->health) - damage <= 0) {
        e->health = 0;
        return TRUE;
    }
    e->health -= damage;
    e->f_invuln = attrs(e).invuln_max_frames;
    return FALSE;
}

int decrementInvulnFrames(ENTITY* e) {
    if (e->f_invuln > 0) {
        e->f_invuln--;
        if (e->f_invuln == 0) {// && IABS(e->dx) == HURT_DX) { // hack to prevent dx being set to 0 when it probably shouldn't
            e->dx = 0;
        }
    }
    return e->f_invuln;
}

INLINE void shooter_checkPlayerCollisions() {
    int8_t playerHurt_tribool = 0;

    int engager_i = 0;
    bool player_has_hurt_enemy = FALSE;
    while (engager_i < objs_length) {
        if (allEntities[engager_i].type != PROJECTILE
                    && allEntities[engager_i].type != PLAYER) {
            engager_i++;
            continue;
        }
        ENTITY* engager = allEntities + engager_i;
        const enum ENTITY_TYPE engager_type = engager->type;
        const uint8_t engager_x = ENT_X(engager);
        const uint8_t engager_y = groundDist(engager);
        const uint8_t engager_width = attrs(engager).width;
        const uint8_t engager_height = attrs(engager).height;

        int i = 1;
        // FIXME: loop of hell
        while (i < objs_length) {
            if (allEntities[i].isDead || (allEntities[i].type == PROJECTILE) || allEntities[i].type == PLAYER) {
                i++;
                continue;
            }

            const uint8_t ent_x = ENT_X(allEntities+i);
            const uint8_t ent_y = groundDist(allEntities + i);
            const ENTITY_ATTRS attrs = attrs(allEntities + i);
            // check bounding boxes
            const uint8_t eheight = attrs.height;
            const uint8_t ewidth  = attrs.width;


            if (engager_type == PLAYER) {
                const uint32_t wider = (ewidth > engager_width) ? ewidth : engager_width;
                const uint32_t taller = (eheight > engager_height) ? eheight : engager_height;

                if (engager_y - ent_y > engager_width - 8
                        && IABS(engager_x - ent_x) <= wider + 5  // make the player head jumping more forgiving
                        && IABS(engager_y - ent_y) <= PLAYER_HEIGHT
                        && engager->dy < 0) {

                    // prevent the player from jumping on multiple enemies in the same frame, but still give them the
                    // jump boost
                    if (!player_has_hurt_enemy) {
                        set_enemy_dead(allEntities + i);
                        score++;
                        player_has_hurt_enemy = TRUE;
                    }
                    if (allEntities[i].type == SHORT_ENEMY) engager->dy = 6;
                    else engager->dy = 4;

                    engager->f_invuln = 10;
                } else if (IABS(engager_x - ent_x) < wider
                        && IABS(engager_y - ent_y) < taller) {

                    playerHurt_tribool = TRIBOOL(engager_x - ent_x);
                    if (playerHurt_tribool == 0) playerHurt_tribool = 1; // guarantee tribool not 0;
                }
            } else if (engager_type == PROJECTILE) {
                if (IABS(engager_x - ent_x) < ewidth
                        && IABS(engager_y - ent_y) < eheight) {

                    if (allEntities[i].type == SHORT_ENEMY) { // projectiles bounce off short enemies
                        engager->dy=5;
                        engager->isDead = TRUE;
                    } else {
                        if (allEntities[i].f_invuln == 0) {
                            const bool killed = hurtEntity(allEntities + i, 1, TRIBOOL(engager_x - ent_x));
                            if (killed) {
                                set_enemy_dead(allEntities + i);
                                score++;
                            }
                        }
                        removeEntity(engager_i--);
                        // missing a break statement was causing tons of bugs.
                        // Was causing all the stored variables for the
                        // projectile to be used with another entity in its
                        // place, causing double hits.
                        // engager_type != engager->type (lol)
                        // A symptom of the fact that this loop is disgusting
                        // and needs refactor.
                        break;
                    }
                }
            }
            i++;
        }
        engager_i++;
    }

    if (playerHurt_tribool && PLAYER_ENTITY->f_invuln == 0) { // the player is newly hurt in this frame
        if (hurtEntity(PLAYER_ENTITY, 1, playerHurt_tribool)) {
            gameOver();
        }
        PLAYER_ENTITY->dx = playerHurt_tribool * 3;
    }
}

void tickEntities(const uint32_t count) {
    if (!(count & 3)) {
        for (int i = 0; i < objs_length; i++) {
            ENTITY* curr = allEntities + i;
            if (entity_attrs[curr->type].gravityEnabled) gravity(curr);
            else OBJ_DY(*curr->obj, ~(curr->dy)+1);

            decrementInvulnFrames(curr);
            if (gameState == SHOOTER) {
                bool stoppedWraparound = FALSE;
                if (ENT_X(curr) + (curr)->dx > SCREEN_WIDTH - attrs(curr).width) { // wrap around
                    BF_SET(curr->obj->attr1, SCREEN_WIDTH - attrs(curr).width, ATTR1_X);
                    stoppedWraparound = TRUE;
                } else if (ENT_X(curr) + curr->dx < 0) { // wrap around
                    BF_SET(curr->obj->attr1, 0, ATTR1_X);
                    stoppedWraparound = TRUE;
                } else {
                    OBJ_DX(curr->obj->attr1, curr->dx);
                }

                // instead of stopping wraparound on projectiles, just remove them
                if (stoppedWraparound && curr->type == PROJECTILE) {
                    removeEntity(i--);
                }
            } else {
                OBJ_DX(curr->obj->attr1, ((curr)->dx + runner_conf->scroller_dx));
            }

            int8_t ent_action_rand = qran_range(0,20);
            if (!curr->isDead) {
                switch (curr->type) {
                    case TALL_ENEMY:
                        if (ent_action_rand == 3 && !curr->isJumping) setJumping(curr, 6);
                        // FALLTHROUGH TO SHORT ENEMY
                    case SHORT_ENEMY:
                        if(!curr->isJumping && ent_action_rand > 16) { // chance to find player again
                            setWalking(curr, TRIBOOL(ENT_X(PLAYER_ENTITY) - ENT_X(curr) ));
                        }
                    default:
                        break;
                }
            }
        }

    }
    switch (gameState) {
        case RUNNER:
            if ((count & 0x3)==2) {
                runner_checkPlayerCollisions();
            }
            break;
        case SHOOTER:
            if ((count & 0x7)==2) {
                shooter_checkPlayerCollisions(count);
            }
            break;
        default:
            break;
    }
}

