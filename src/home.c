#include "angband.h"
#include "jsmn.h"

#include <assert.h>

static inv_ptr _home = NULL;
static inv_ptr _museum = NULL;

/* Function to convert a hex string representation back to an object
   This is useful for debugging or object deserialization */
bool hex_to_obj(cptr hex_str, obj_ptr obj)
{
    byte *p = (byte *)obj;
    size_t i, len;
    
    /* Check for NULL pointers */
    if (!hex_str || !obj) return FALSE;
    
    /* Get the string length */
    len = strlen(hex_str);
    
    /* Ensure the string is valid (even length, contains only hex chars) */
    if (len % 2 != 0) return FALSE;
    
    /* Clear the object first */
    object_wipe(obj);
    
    /* Convert hex pairs back to bytes */
    for (i = 0; i < len / 2 && i < sizeof(obj_t); i++) 
    {
        byte hi, lo;
        
        /* Extract hex digits */
        if (hex_str[i*2] >= '0' && hex_str[i*2] <= '9')
            hi = hex_str[i*2] - '0';
        else if (hex_str[i*2] >= 'A' && hex_str[i*2] <= 'F')
            hi = hex_str[i*2] - 'A' + 10;
        else if (hex_str[i*2] >= 'a' && hex_str[i*2] <= 'f')
            hi = hex_str[i*2] - 'a' + 10;
        else
            return FALSE;

        if (hex_str[i*2+1] >= '0' && hex_str[i*2+1] <= '9')
            lo = hex_str[i*2+1] - '0';
        else if (hex_str[i*2+1] >= 'A' && hex_str[i*2+1] <= 'F')
            lo = hex_str[i*2+1] - 'A' + 10;
        else if (hex_str[i*2+1] >= 'a' && hex_str[i*2+1] <= 'f')
            lo = hex_str[i*2+1] - 'a' + 10;
        else
            return FALSE;
            
        /* Combine the nibbles */
        p[i] = (hi << 4) | lo;
    }
    
    /* Success */
    return TRUE;
}

/* Function to dump an object to a hex string representation
   This is useful for debugging or object serialization */
void obj_dump_hex(obj_ptr obj, char *buf, size_t max)
{
    byte *p = (byte *)obj;
    size_t i, size = sizeof(obj_t);
    char *q = buf;
    
    /* Ensure we have at least some space */
    if (max < 3) {
        if (max > 0) *buf = '\0';
        return;
    }
    
    /* Cap at maximum available buffer space, allowing for null termination */
    if (size * 2 >= max) size = (max - 1) / 2;
    
    /* Convert each byte to a 2-digit hex representation */
    for (i = 0; i < size; i++)
    {
        byte b = *p++;
        *q++ = hexsym[(b & 0xF0) >> 4];
        *q++ = hexsym[b & 0x0F];
    }
    
    /* Null terminate the string */
    *q = '\0';
}

void home_init(void)
{
    inv_free(_home);
    inv_free(_museum);

    _home = inv_alloc("Home", INV_HOME, 0);
    _museum = inv_alloc("Museum", INV_MUSEUM, 0);
}

inv_ptr home_filter(obj_p p)
{
    return inv_filter(_home, p);
}

obj_ptr home_obj(slot_t slot)
{
    return inv_obj(_home, slot);
}

int home_max(void)
{
    return inv_max(_home);
}

void home_for_each(obj_f f)
{
    inv_for_each(_home, f);
}

void home_optimize(void)
{
    inv_optimize(_home);
}

void home_carry(obj_ptr obj)
{
    if (obj->number)
        inv_combine_ex(_home, obj);
    if (obj->number)
        inv_add(_home, obj);
}

static void museum_carry(obj_ptr obj)
{

    obj->inscription = 0; // Clear inscription
    if (obj->number)
        inv_combine_ex(_museum, obj);
    if (obj->number)
        inv_add(_museum, obj);
}

/************************************************************************
 * Character Sheet (py_display)
 ***********************************************************************/

void home_display(doc_ptr doc, obj_p p, int flags)
{
    inv_ptr inv = inv_filter(_home, obj_exists);
    char    name[MAX_NLEN];
    slot_t  slot;
    slot_t  max = inv_count_slots(inv, obj_exists);

    inv_sort(inv);

    for (slot = 1; slot <= max; slot++)
    {
        obj_ptr obj = inv_obj(inv, slot);
        if (!obj) continue; /* bug */
        object_desc(name, obj, OD_COLOR_CODED);
        if (!obj->scratch) obj->scratch = obj_value(obj);
        doc_printf(doc, "<color:R>%6d</color> <indent><style:indent>%s</style></indent>\n", obj->scratch, name);
    }
    inv_free(inv);
}

int home_count(obj_p p)
{
    return inv_count(_home, p);
}

int museum_count(obj_p p)
{
    return inv_count(_museum, p);
}

void museum_display(doc_ptr doc, obj_p p, int flags)
{
    slot_t slot;
    slot_t max = inv_last(_museum, obj_exists);
    char   name[MAX_NLEN];

    http_response_t response;


    for (slot = 1; slot <= max; slot++)
    {
        obj_ptr obj = inv_obj(_museum, slot);
        if (!obj) continue; /* bug */
        object_desc(name, obj, OD_COLOR_CODED);
        doc_printf(doc, "<indent><style:indent>%s</style></indent>\n", name);
    }

}

/************************************************************************
 * Savefiles
 ***********************************************************************/
void home_load(savefile_ptr file)
{
    inv_load(_home, file);
    inv_load(_museum, file);
}

void home_save(savefile_ptr file)
{
    inv_save(_home, file);
    inv_save(_museum, file);
}

/************************************************************************
 * User Interface
 ***********************************************************************/
struct _ui_context_s
{
    inv_ptr inv;
    slot_t  top;
    int     page_size;
    doc_ptr doc;
};
typedef struct _ui_context_s _ui_context_t, *_ui_context_ptr;

static void _display(_ui_context_ptr context);
static void _drop(_ui_context_ptr context);
static void _remove(_ui_context_ptr context);
static void _examine(_ui_context_ptr context);
static void _get(_ui_context_ptr context);
static void _ui(_ui_context_ptr context);
static void _drop_aux(obj_ptr obj, _ui_context_ptr context);
static void _fetch_museum_data(_ui_context_ptr context);

void home_ui(void)
{
    _ui_context_t context = {0};

    context.inv = _home;
    context.top = 1;

    _ui(&context);
}

void museum_ui(void)
{
    _ui_context_t context = {0};
    _fetch_museum_data(&context); /* Fetch museum data from server */

    context.inv = _museum;
    context.top = 1;
    _ui(&context);
}

static void _fetch_museum_data(_ui_context_ptr context)
{
    _museum = inv_alloc("Museum", INV_MUSEUM, 0);
    //  make http post request
    http_response_t response;
    bool success = make_http_request("http://120.78.193.74/frogcomposbandnet/share_room/list", NULL, &response);
    if (success && response.data)
    {
        jsmn_parser parser;
        jsmntok_t tokens[128];
        int token_count;

        jsmn_init(&parser);
        token_count = jsmn_parse(&parser, response.data, response.size, tokens, sizeof(tokens)/sizeof(tokens[0]));

        
        // json example:
        // [{"hex": "000102030405060708090A0B0C0D0E0F", "number": 1}, {"hex": "101112131415161718191A1B1C1D1E1F", "number": 2}]
        // where each object has a "hex" field with a hex string and a "number" field
        if (token_count > 0)
        {
            for(int i = 0; i < token_count; i++)
            {
                if (tokens[i].type != JSMN_OBJECT) continue;

                // Find "hex" and "number" fields
                char hex_buf[2048];
                int number = 0;
                for (;;) {
                    i++;
                    if (i >= token_count) break; // Prevent out of bounds
                    if (tokens[i].type == JSMN_STRING && tokens[i].size == 1) {
                        // Check for "hex" field
                        if (strncmp(response.data + tokens[i].start, "hex", tokens[i].end - tokens[i].start) == 0) {
                            i++; // Move to the value
                            strncpy(hex_buf, response.data + tokens[i].start, tokens[i].end - tokens[i].start);
                            hex_buf[tokens[i].end - tokens[i].start] = '\0';
                        }
                        // Check for "number" field
                        else if (strncmp(response.data + tokens[i].start, "number", tokens[i].end - tokens[i].start) == 0) {
                            i++; // Move to the value
                            number = atoi(response.data + tokens[i].start);
                        }
                    }
                    if (i >= token_count || tokens[i].type != JSMN_STRING) break; // Exit loop if not a string
                }

                if (hex_buf)
                {
                    obj_ptr obj = obj_alloc();
                    if (hex_to_obj(hex_buf, obj))
                    {
                        obj->number = number;
                        museum_carry(obj);
                    }
                }
            }

            inv_sort(_museum);
        }
        free(response.data);
    }
}

static void _sync_drop(_ui_context_ptr context, obj_ptr obj) {
    http_response_t response;
    char post_data[1024];
    char name[MAX_NLEN];
    object_desc(name, obj, OD_OMIT_INSCRIPTION);

    char hex_buf[2048]; /* Should be more than enough for an obj_t */
    obj_dump_hex(obj, hex_buf, sizeof(hex_buf));
    // set up the POST data
    snprintf(post_data, sizeof(post_data), "{\"name\": \"%s\", \"hex\": \"%s\", \"k_idx\": %d, \"tval\": %d, \"sval\": %d, \"pval\": %d, \"name2\": %d, \"name3\": %d, \"number\": %d}",
     name, hex_buf, obj->k_idx, obj->tval, obj->sval, obj->pval, obj->name2, obj->name3, obj->number);

    //  make http post request
    bool success = make_http_post("http://120.78.193.74/frogcomposbandnet/share_room/drop", &post_data, &response);
    free(response.data);
}

static void _sync_get(obj_ptr obj) {
    http_response_t response;
    char post_data[1024];
    char name[MAX_NLEN];
    object_desc(name, obj, OD_COLOR_CODED);

    char hex_buf[2048]; /* Should be more than enough for an obj_t */
    obj_dump_hex(obj, hex_buf, sizeof(hex_buf));
    // set up the POST data
    snprintf(post_data, sizeof(post_data), "{\"k_idx\": %d, \"tval\": %d, \"sval\": %d, \"pval\": %d, \"name2\": %d, \"name3\": %d, \"number\": %d}",
     obj->k_idx, obj->tval, obj->sval, obj->pval, obj->name2, obj->name3, obj->number);

    //  make http post request
    bool success = make_http_post("http://120.78.193.74/frogcomposbandnet/share_room/get", &post_data, &response);
    free(response.data);
}

static void _ui(_ui_context_ptr context)
{
    forget_lite(); /* resizing the term would redraw the map ... sigh */
    forget_view();
    character_icky = TRUE;

    msg_line_clear();
    msg_line_init(ui_shop_msg_rect());

    Term_clear();
    context->doc = doc_alloc(MIN(80, ui_shop_rect().cx));
    for (;;)
    {
        int    max = inv_last(context->inv, obj_exists);
        rect_t r = ui_shop_rect(); /* recalculate in case resize */
        int    cmd, ct;

        context->page_size = MIN(26, r.cy - 3 - 4);
        if ((context->top - 1) % context->page_size != 0) /* resize?? */
            context->top = 1;

        _display(context);

        cmd = inkey_special(TRUE);
        msg_line_clear();
        msg_boundary(); /* turn_count is unchanging while in home/museum */
        if (cmd == ESCAPE || cmd == 'q' || cmd == 'Q') break;
        pack_lock();
        if (!shop_common_cmd_handler(cmd))
        {
            switch (cmd)
            {
            case 'g': case 'b': case 'p':  _get(context); break;
            case 'd': case 's':  _drop(context); break;
            case 'x': _examine(context); break;
            case 'r': _remove(context); break;
            case '?':
                doc_display_help("context_home.txt", inv_loc(context->inv) == INV_MUSEUM ? "Museum" : NULL);
                Term_clear_rect(ui_shop_msg_rect());
                break;
            case SKEY_PGDOWN: case '3': case ' ':
                if (context->top + context->page_size - 1 < max)
                    context->top += context->page_size;
                break;
            case SKEY_PGUP: case '9': case '-':
                if (context->top > context->page_size)
                    context->top -= context->page_size;
                break;
            case SKEY_BOTTOM: case '1':
                 while (context->top + context->page_size - 1 < max)
                 {
                    context->top += context->page_size;
                 }
                 break;
            case SKEY_TOP: case '7':
                 context->top = 1;
                 break;
            default:
                if (cmd < 256 && isprint(cmd))
                {
                    msg_format("Unrecognized command: <color:R>%c</color>. "
                               "Press <color:keypress>?</color> for help.", cmd);
                }
                else if (KTRL('A') <= cmd && cmd <= KTRL('Z'))
                {
                    cmd |= 0x40;
                    msg_format("Unrecognized command: <color:R>^%c</color>. "
                               "Press <color:keypress>?</color> for help.", cmd);
                }
            }
            ct = inv_count_slots(context->inv, obj_exists);
            if (ct)
            {
                max = inv_last(context->inv, obj_exists);
                while (context->top > max)
                    context->top -= context->page_size;
                if (context->top < 1) context->top = 1;
            }
        }
        pack_unlock();
        notice_stuff(); /* PW_INVEN and PW_PACK ... */
        handle_stuff(); /* Plus 'C' to view character sheet */
        if ((shop_exit_hack) || (pack_overflow_count() > ((pack_is_full()) ? 0 : 1)))
        {
            if (shop_exit_hack) msg_print("It's time for you to leave!");
            else msg_print("<color:v>Your pack is overflowing!</color> It's time for you to leave!");
            msg_print(NULL);
            shop_exit_hack = FALSE;
            break;
        }
    }
    character_icky = FALSE;
    energy_use = 100;
    msg_line_clear();
    msg_line_init(ui_msg_rect());

    Term_clear();
    do_cmd_redraw();

    doc_free(context->doc);
}

static void _display(_ui_context_ptr context)
{
    rect_t  r = ui_shop_rect();
    doc_ptr doc = context->doc;

    doc_clear(doc);
    doc_insert(doc, "<style:table>");
    doc_printf(doc, "%*s<color:G>%s</color>\n\n",
        (doc_width(doc) - 10)/2, "", inv_name(context->inv));

    shop_display_inv(doc, context->inv, context->top, context->page_size);
    
    {
        slot_t max = inv_last(context->inv, obj_exists);
        slot_t bottom = context->top + context->page_size - 1;

        if (context->top > 1 || bottom < max)
        {
            int page_count = (max - 1) / context->page_size + 1;
            int page_current = (context->top - 1) / context->page_size + 1;

            doc_printf(doc, "<color:B>(Page %d of %d)</color>\n", page_current, page_count);
        }
        else
            doc_newline(doc);
    }
    if (inv_loc(context->inv) == INV_HOME)
    {
        doc_insert(doc,
            "<color:keypress>g</color> to get an item. "
            "<color:keypress>d</color> to drop an item. ");
    }
    else
        doc_insert(doc, "<color:keypress>d</color> to donate an item. ");
    
    doc_insert(doc,
        "<color:keypress>x</color> to begin examining items.\n"
        "<color:keypress>r</color> to begin removing (destroying) items.\n"
        "<color:keypress>Esc</color> to exit. "
        "<color:keypress>?</color> for help.");
    doc_insert(doc, "</style>");

    Term_clear_rect(r);
    doc_sync_term(doc,
        doc_range_top_lines(context->doc, r.cy),
        doc_pos_create(r.x, r.y));
}

static void _get_aux(obj_ptr obj)
{
    /*char name[MAX_NLEN];
    object_desc(name, obj, OD_COLOR_CODED);
    msg_format("You get %s.", name);*/
    pack_carry(obj);
}

static void _get(_ui_context_ptr context)
{
    for (;;)
    {
        char    cmd;
        slot_t  slot;
        obj_ptr obj;
        int     amt = 1;

        if (!msg_command("<color:y>Get which item <color:w>(<color:keypress>Esc</color> "
                         "to cancel)</color>?</color>", &cmd)) break;
        if (cmd < 'a' || cmd > 'z') continue;
        slot = label_slot(cmd);
        slot = slot + context->top - 1;
        obj = inv_obj(context->inv, slot);
        if (!obj) continue;

        if (obj->number > 1)
        {
            if (!msg_input_num("Quantity", &amt, 1, obj->number)) continue;
        }
        if (amt < obj->number)
        {
            obj_t copy = *obj;
            copy.number = amt;
            obj->number -= amt;
            if (obj->insured)
            {
                int vahennys = MIN(amt, (obj->insured % 100));
                copy.insured = (obj->insured / 100) * 100 + vahennys;
                obj_dec_insured(obj, vahennys);
            }
            if (inv_loc(context->inv) == INV_MUSEUM)
                _sync_get(&copy);
            _get_aux(&copy);
        }
        else
        {
            if (inv_loc(context->inv) == INV_MUSEUM)
                _sync_get(obj);
            _get_aux(obj);
            if (!obj->number)
            {
                inv_remove(context->inv, slot);
                inv_sort(context->inv);
            }
        }
        break;
    }
}

static void _drop_aux(obj_ptr obj, _ui_context_ptr context)
{
    char name[MAX_NLEN];
    if (object_is_(obj, TV_POTION, SV_POTION_BLOOD))
    {
        msg_print("The potion goes sour.");
        obj->sval = SV_POTION_SALT_WATER;
        obj->k_idx = lookup_kind(TV_POTION, SV_POTION_SALT_WATER);
        object_origins(obj, ORIGIN_BLOOD);
        obj->mitze_type = 0;
    }
    object_desc(name, obj, OD_COLOR_CODED);
    if (inv_loc(context->inv) == INV_MUSEUM)
    {
        msg_format("You donate %s.", name);

        museum_carry(obj);
        inv_sort(_museum);
        virtue_add(VIRTUE_SACRIFICE, 1); /* TODO: should depend on obj_value() */
    }
    else
    {
        msg_format("You drop %s.", name);
        home_carry(obj);
        inv_sort(_home);
    }
}

static void _drop(_ui_context_ptr context)
{
    obj_prompt_t prompt = {0};
    int          amt = 1;
    bool         new_id = FALSE;

    if (inv_loc(context->inv) == INV_MUSEUM)
    {
        prompt.prompt = "Donate which item?";
        prompt.error = "You have nothing to donate.";
    }
    else
    {
        prompt.prompt = "Drop which item?";
        prompt.error = "You have nothing to drop.";
    }
    prompt.where[0] = INV_PACK;
    prompt.where[1] = INV_EQUIP;
    prompt.where[2] = INV_QUIVER;
    obj_prompt_add_special_packs(&prompt);

    obj_prompt(&prompt);
    if (!prompt.obj) return;

    if (obj_is_true_art(prompt.obj) && inv_loc(context->inv) == INV_MUSEUM)
    {
        msg_print("You cannot donate true artifacts to the museum.");
        return;
    }

    if (prompt.obj->loc.where == INV_EQUIP)
    {
        if (prompt.obj->tval == TV_QUIVER && quiver_count(NULL))
        {
            msg_print("Your quiver still holds ammo. Remove all the ammo from your quiver first.");
            return;
        }
        if (!equip_can_takeoff(prompt.obj)) return;
    }

    if (inv_loc(context->inv) == INV_MUSEUM)
    {
        char       name[MAX_NLEN];
        string_ptr s = string_copy_s("<color:v>Warning:</color> All donations are final! ");
        char       c;

        object_desc(name, prompt.obj, OD_COLOR_CODED);
        string_printf(s, "Really donate %s to the museum? <color:y>[y/n]</color>", name);
        c = msg_prompt(string_buffer(s), "ny", PROMPT_YES_NO);
        string_free(s);
        if (c == 'n') return;
    }
    else
        amt = prompt.obj->number;

    if (prompt.obj->number > 1)
    {
        if (!msg_input_num("Quantity", &amt, 1, prompt.obj->number)) return;
    }

    // if (inv_loc(context->inv) == INV_MUSEUM)
    // {
    //     if (!object_is_known(prompt.obj)) new_id = TRUE;
    //     /* *identify* here rather than in _drop_aux in case the user splits a pile. */
    //     no_karrot_hack = TRUE;
    //     obj_identify_fully(prompt.obj);
    //     no_karrot_hack = FALSE;
    // }

    if (prompt.obj->loc.where == INV_EQUIP)
    {
        char name[MAX_NLEN];
        object_desc(name, prompt.obj, OD_COLOR_CODED);
        msg_format("You are no longer wearing %s.", name);
        p_ptr->update |= PU_BONUS | PU_TORCH | PU_MANA;
        p_ptr->redraw |= PR_EQUIPPY;
        p_ptr->window |= PW_EQUIP;        
    }

    if (amt < prompt.obj->number)
    {
        obj_t copy = *prompt.obj;
        copy.number = amt;
        prompt.obj->number -= amt;
        if (prompt.obj->insured)
        {
            copy.insured = 0;
            if ((prompt.obj->insured % 100) > prompt.obj->number)
            {
                int vahennys = (prompt.obj->insured % 100) - prompt.obj->number;
                copy.insured = prompt.obj->insured / 100 * 100 + vahennys;
                obj_dec_insured(prompt.obj, vahennys);
            }
        }
        if (inv_loc(context->inv) == INV_MUSEUM)
            _sync_drop(context, &copy);
        _drop_aux(&copy, context);
        if (new_id) autopick_alter_obj(prompt.obj, ((destroy_identify) && (obj_value(prompt.obj) < 1)));
    }
    else
        if (inv_loc(context->inv) == INV_MUSEUM)
            _sync_drop(context, prompt.obj);
        _drop_aux(prompt.obj, context);

    obj_release(prompt.obj, OBJ_RELEASE_QUIET);
}

static void _examine(_ui_context_ptr context)
{
    for (;;)
    {
        char    cmd;
        slot_t  slot;
        obj_ptr obj;

        if (!msg_command("<color:y>Examine which item <color:w>(<color:keypress>Esc</color> when done)</color>?</color>", &cmd)) break;
        if (cmd < 'a' || cmd > 'z') continue;
        slot = label_slot(cmd);
        slot = slot + context->top - 1;
        obj = inv_obj(context->inv, slot);
        if (!obj) continue;

        obj_display(obj);
    }
}

static void _remove(_ui_context_ptr context)
{
    for (;;)
    {
        char    cmd;
        slot_t  slot;
        obj_ptr obj;
        char    name[MAX_NLEN];

        if (!msg_command("<color:y>Remove which item <color:w>(<color:keypress>Esc</color> when done)</color>?</color>", &cmd)) break;
        if (cmd < 'a' || cmd > 'z') continue;
        slot = label_slot(cmd);
        slot = slot + context->top - 1;
        obj = inv_obj(context->inv, slot);
        if (!obj) continue;

        object_desc(name, obj, OD_COLOR_CODED);
        cmd = msg_prompt(format("<color:y>Really remove %s?</color> <color:v>It will "
            "be permanently destroyed!</color> <color:y>[Y,n]</color>", name), "ny", PROMPT_YES_NO);
        if (cmd == 'n') continue;
        if (!can_player_destroy_object(obj))
        {
            object_desc(name, obj, OD_COLOR_CODED);
            msg_format("You cannot destroy %s.", name);
            continue;
        }
        inv_remove(context->inv, slot);
        inv_sort(context->inv);
        _display(context);
    }
}

