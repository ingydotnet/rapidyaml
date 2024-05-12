#ifndef _C4_YML_EVENT_HANDLER_TREE_HPP_
#define _C4_YML_EVENT_HANDLER_TREE_HPP_

#ifndef _C4_YML_TREE_HPP_
#include "c4/yml/tree.hpp"
#endif

#ifndef _C4_YML_EVENT_HANDLER_STACK_HPP_
#include "c4/yml/event_handler_stack.hpp"
#endif

C4_SUPPRESS_WARNING_MSVC_WITH_PUSH(4702) // unreachable code

namespace c4 {
namespace yml {

/** @addtogroup doc_event_handlers
 * @{ */


/** The event handler to create a ryml @ref Tree. See the
 * documentation for @ref doc_event_handlers, which has important
 * notes about the event model used by rapidyaml. */
struct EventHandlerTree : public EventHandlerStack<EventHandlerTree, ParserState>
{

    /** @name types
     * @{ */

    using state = ParserState;

    /** @} */

public:

    /** @cond dev */
    Tree *C4_RESTRICT m_tree;
    id_type m_id;

    #if RYML_DBG
    #define _enable_(bits) _enable__<bits>(); _c4dbgpf("node[{}]: enable {}", m_curr->node_id, #bits)
    #define _disable_(bits) _disable__<bits>(); _c4dbgpf("node[{}]: disable {}", m_curr->node_id, #bits)
    #else
    #define _enable_(bits) _enable__<bits>()
    #define _disable_(bits) _disable__<bits>()
    #endif
    #define _has_any_(bits) _has_any__<bits>()
    /** @endcond */

public:

    /** @name construction and resetting
     * @{ */

    EventHandlerTree() : EventHandlerStack(), m_tree(), m_id(NONE) {}
    EventHandlerTree(Callbacks const& cb) : EventHandlerStack(cb), m_tree(), m_id(NONE) {}
    EventHandlerTree(Tree *tree, id_type id) : EventHandlerStack(tree->callbacks()), m_tree(tree), m_id(id)
    {
        reset(tree, id);
    }

    void reset(Tree *tree, id_type id)
    {
        RYML_CHECK(tree);
        RYML_CHECK(id < tree->capacity());
        if(!tree->is_root(id))
            if(tree->is_map(tree->parent(id)))
                if(!tree->has_key(id))
                    c4::yml::error("destination node belongs to a map and has no key");
        m_tree = tree;
        m_id = id;
        if(m_tree->is_root(id))
        {
            _stack_reset_root();
            _reset_parser_state(m_curr, id, m_tree->root_id());
        }
        else
        {
            _stack_reset_non_root();
            _reset_parser_state(m_parent, id, m_tree->parent(id));
            _reset_parser_state(m_curr, id, id);
        }
    }

    /** @} */

public:

    /** @name parse events
     * @{ */

    void start_parse(const char* filename)
    {
        m_curr->start_parse(filename, m_curr->node_id);
    }

    void finish_parse()
    {
        /* This pointer is temporary. Remember that:
         *
         * - this handler object may be held by the user
         * - it may be used with a temporary tree inside the parse function
         * - when the parse function returns the temporary tree, its address
         *   will change
         *
         * As a result, the user could try to read the tree from m_tree, and
         * end up reading the stale temporary object.
         *
         * So it is better to clear it here; then the user will get an obvious
         * segfault to read from m_tree. */
        m_tree = nullptr;
    }

    void cancel_parse()
    {
        m_tree = nullptr;
    }

    /** @} */

public:

    /** @name YAML stream events */
    /** @{ */

    C4_ALWAYS_INLINE void begin_stream() const noexcept { /*nothing to do*/ }

    C4_ALWAYS_INLINE void end_stream() const noexcept { /*nothing to do*/ }

    /** @} */

public:

    /** @name YAML document events */
    /** @{ */

    /** implicit doc start (without ---) */
    void begin_doc()
    {
        _c4dbgp("begin_doc");
        if(_stack_should_push_on_begin_doc())
        {
            _c4dbgp("push!");
            _set_root_as_stream();
            _push();
            _enable_(DOC);
        }
    }
    /** implicit doc end (without ...) */
    void end_doc()
    {
        _c4dbgp("end_doc");
        if(_stack_should_pop_on_end_doc())
        {
            _remove_speculative();
            _c4dbgp("pop!");
            _pop();
        }
    }

    /** explicit doc start, with --- */
    void begin_doc_expl()
    {
        _c4dbgp("begin_doc_expl");
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->root_id() == m_curr->node_id);
        if(!m_tree->is_stream(m_tree->root_id())) //if(_should_push_on_begin_doc())
        {
            _c4dbgp("ensure stream");
            _set_root_as_stream();
            id_type first = m_tree->first_child(m_tree->root_id());
            _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->is_stream(m_tree->root_id()));
            _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->num_children(m_tree->root_id()) == 1u);
            if(m_tree->has_children(first) || m_tree->is_val(first))
            {
                _c4dbgp("push!");
                _push();
            }
            else
            {
                _c4dbgp("tweak");
                _push();
                _remove_speculative();
                m_curr->node_id = m_tree->last_child(m_tree->root_id());
            }
        }
        else
        {
            _c4dbgp("push!");
            _push();
        }
        _enable_(DOC);
    }
    /** explicit doc end, with ... */
    void end_doc_expl()
    {
        _c4dbgp("end_doc_expl");
        _remove_speculative();
        if(_stack_should_pop_on_end_doc())
        {
            _c4dbgp("pop!");
            _pop();
        }
    }

    /** @} */

public:

    /** @name YAML map events */
    /** @{ */

    void begin_map_key_flow()
    {
        _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
    }
    void begin_map_key_block()
    {
        _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
    }

    void begin_map_val_flow()
    {
        _c4dbgpf("node[{}]: begin_map_val_flow", m_curr->node_id);
        _RYML_CB_CHECK(m_stack.m_callbacks, !_has_any_(VAL));
        _enable_(MAP|FLOW_SL);
        _save_loc();
        _push();
    }
    void begin_map_val_block()
    {
        _c4dbgpf("node[{}]: begin_map_val_block", m_curr->node_id);
        _RYML_CB_CHECK(m_stack.m_callbacks, !_has_any_(VAL));
        _enable_(MAP|BLOCK);
        _save_loc();
        _push();
    }

    void end_map()
    {
        _pop();
        _c4dbgpf("node[{}]: end_map_val", m_curr->node_id);
    }

    /** @} */

public:

    /** @name YAML seq events */
    /** @{ */

    void begin_seq_key_flow()
    {
        _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
    }
    void begin_seq_key_block()
    {
        _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
    }

    void begin_seq_val_flow()
    {
        _c4dbgpf("node[{}]: begin_seq_val_flow", m_curr->node_id);
        _RYML_CB_CHECK(m_stack.m_callbacks, !_has_any_(VAL));
        _enable_(SEQ|FLOW_SL);
        _save_loc();
        _push();
    }
    void begin_seq_val_block()
    {
        _c4dbgpf("node[{}]: begin_seq_val_block", m_curr->node_id);
        _RYML_CB_CHECK(m_stack.m_callbacks, !_has_any_(VAL));
        _enable_(SEQ|BLOCK);
        _save_loc();
        _push();
    }

    void end_seq()
    {
        _pop();
        _c4dbgpf("node[{}]: end_seq_val", m_curr->node_id);
    }

    /** @} */

public:

    /** @name YAML structure events */
    /** @{ */

    void add_sibling()
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_parent);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->has_children(m_parent->node_id));
        _set_state_(m_curr, m_tree->_append_child__unprotected(m_parent->node_id));
        _c4dbgpf("node[{}]: added sibling={} prev={}", m_parent->node_id, m_curr->node_id, m_tree->prev_sibling(m_curr->node_id));
    }

    /** set the previous val as the first key of a new map, with flow style.
     *
     * See the documentation for @ref doc_event_handlers, which has
     * important notes about this event.
     */
    void actually_val_is_first_key_of_new_map_flow()
    {
        _c4dbgpf("node[{}]: actually first key of new map. parent={}", m_curr->node_id, m_parent ? m_parent->node_id : NONE);
        id_type id = m_curr->node_id;
        if(C4_UNLIKELY(m_tree->is_container(id)))
            _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_parent);
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->is_seq(m_parent->node_id));
        _RYML_CB_ASSERT(m_stack.m_callbacks, !m_tree->is_container(id));
        _RYML_CB_ASSERT(m_stack.m_callbacks, !m_tree->has_key(id));
        // save type and val before changing the tree
        NodeType type = m_tree->m_type[id];
        const csubstr val = m_tree->m_val[id];
        const csubstr tag = m_tree->m_val_tag[id];
        const csubstr anc = m_tree->m_val_anchor[id];
        static_assert((_VALMASK >> 1u) == _KEYMASK, "required for this function to work");
        static_assert((VAL_STYLE >> 1u) == KEY_STYLE, "required for this function to work");
        type = ((type & (_VALMASK|VAL_STYLE)) >> 1u);
        type = (type & ~(_VALMASK|VAL_STYLE));
        type = (type | KEY);
        _disable_(_VALMASK|VAL_STYLE);
        begin_map_val_flow();
        id = m_curr->node_id;
        m_tree->m_type[id] = type;
        m_tree->m_key[id] = val;
        m_tree->m_key_tag[id] = tag;
        m_tree->m_key_anchor[id] = anc;
    }

    /** like its flow counterpart, but this function can only be
     * called after the end of a flow-val at root or doc level.
     *
     * See the documentation for @ref doc_event_handlers, which has
     * important notes about this event.
     */
    void actually_val_is_first_key_of_new_map_block()
    {
        _RYML_CB_ERR_(m_stack.m_callbacks, "ryml trees cannot handle containers as keys", m_curr->pos);
    }

    /** @} */

public:

    /** @name YAML scalar events */
    /** @{ */


    C4_ALWAYS_INLINE void set_key_scalar_plain(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar plain: [{}]~~~{}~~~ ({})", m_curr->node_id, scalar.len, scalar, reinterpret_cast<void const*>(scalar.str));
        m_tree->m_key[m_curr->node_id] = scalar;
        _enable_(KEY|KEY_PLAIN);
    }
    C4_ALWAYS_INLINE void set_val_scalar_plain(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar plain: [{}]~~~{}~~~ ({})", m_curr->node_id, scalar.len, scalar, reinterpret_cast<void const*>(scalar.str));
        m_tree->m_val[m_curr->node_id] = scalar;
        _enable_(VAL|VAL_PLAIN);
    }


    C4_ALWAYS_INLINE void set_key_scalar_dquoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar dquot: [{}]~~~{}~~~ ({})", m_curr->node_id, scalar.len, scalar, reinterpret_cast<void const*>(scalar.str));
        m_tree->m_key[m_curr->node_id] = scalar;
        _enable_(KEY|KEY_DQUO);
    }
    C4_ALWAYS_INLINE void set_val_scalar_dquoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar dquot: [{}]~~~{}~~~ ({})", m_curr->node_id, scalar.len, scalar, reinterpret_cast<void const*>(scalar.str));
        m_tree->m_val[m_curr->node_id] = scalar;
        _enable_(VAL|VAL_DQUO);
    }


    C4_ALWAYS_INLINE void set_key_scalar_squoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar squot: [{}]~~~{}~~~ ({})", m_curr->node_id, scalar.len, scalar, reinterpret_cast<void const*>(scalar.str));
        m_tree->m_key[m_curr->node_id] = scalar;
        _enable_(KEY|KEY_SQUO);
    }
    C4_ALWAYS_INLINE void set_val_scalar_squoted(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar squot: [{}]~~~{}~~~ ({})", m_curr->node_id, scalar.len, scalar, reinterpret_cast<void const*>(scalar.str));
        m_tree->m_val[m_curr->node_id] = scalar;
        _enable_(VAL|VAL_SQUO);
    }


    C4_ALWAYS_INLINE void set_key_scalar_literal(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar literal: [{}]~~~{}~~~ ({})", m_curr->node_id, scalar.len, scalar, reinterpret_cast<void const*>(scalar.str));
        m_tree->m_key[m_curr->node_id] = scalar;
        _enable_(KEY|KEY_LITERAL);
    }
    C4_ALWAYS_INLINE void set_val_scalar_literal(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar literal: [{}]~~~{}~~~ ({})", m_curr->node_id, scalar.len, scalar, reinterpret_cast<void const*>(scalar.str));
        m_tree->m_val[m_curr->node_id] = scalar;
        _enable_(VAL|VAL_LITERAL);
    }


    C4_ALWAYS_INLINE void set_key_scalar_folded(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set key scalar folded: [{}]~~~{}~~~ ({})", m_curr->node_id, scalar.len, scalar, reinterpret_cast<void const*>(scalar.str));
        m_tree->m_key[m_curr->node_id] = scalar;
        _enable_(KEY|KEY_FOLDED);
    }
    C4_ALWAYS_INLINE void set_val_scalar_folded(csubstr scalar) noexcept
    {
        _c4dbgpf("node[{}]: set val scalar folded: [{}]~~~{}~~~ ({})", m_curr->node_id, scalar.len, scalar, reinterpret_cast<void const*>(scalar.str));
        m_tree->m_val[m_curr->node_id] = scalar;
        _enable_(VAL|VAL_FOLDED);
    }


    C4_ALWAYS_INLINE void mark_key_scalar_unfiltered() noexcept
    {
        _enable_(KEY_UNFILT);
    }
    C4_ALWAYS_INLINE void mark_val_scalar_unfiltered() noexcept
    {
        _enable_(VAL_UNFILT);
    }

    /** @} */

public:

    /** @name YAML anchor/reference events */
    /** @{ */

    void set_key_anchor(csubstr anchor) RYML_NOEXCEPT
    {
        _c4dbgpf("node[{}]: set key anchor: [{}]~~~{}~~~", m_curr->node_id, anchor.len, anchor);
        _RYML_CB_ASSERT(m_stack.m_callbacks, !anchor.begins_with('&'));
        _enable_(KEYANCH);
        m_tree->m_key_anchor[m_curr->node_id] = anchor;
    }
    void set_val_anchor(csubstr anchor) RYML_NOEXCEPT
    {
        _c4dbgpf("node[{}]: set val anchor: [{}]~~~{}~~~", m_curr->node_id, anchor.len, anchor);
        _RYML_CB_ASSERT(m_stack.m_callbacks, !anchor.begins_with('&'));
        _enable_(VALANCH);
        m_tree->m_val_anchor[m_curr->node_id] = anchor;
    }

    void set_key_ref(csubstr ref) RYML_NOEXCEPT
    {
        _c4dbgpf("node[{}]: set key ref: [{}]~~~{}~~~", m_curr->node_id, ref.len, ref);
        _RYML_CB_ASSERT(m_stack.m_callbacks, ref.begins_with('*'));
        _enable_(KEY|KEYREF);
        m_tree->m_key_anchor[m_curr->node_id] = ref.sub(1);
        m_tree->m_key[m_curr->node_id] = ref;
    }
    void set_val_ref(csubstr ref) RYML_NOEXCEPT
    {
        _c4dbgpf("node[{}]: set val ref: [{}]~~~{}~~~", m_curr->node_id, ref.len, ref);
        _RYML_CB_ASSERT(m_stack.m_callbacks, ref.begins_with('*'));
        _enable_(VAL|VALREF);
        m_tree->m_val_anchor[m_curr->node_id] = ref.sub(1);
        m_tree->m_val[m_curr->node_id] = ref;
    }

    /** @} */

public:

    /** @name YAML tag events */
    /** @{ */

    void set_key_tag(csubstr tag) noexcept
    {
        _c4dbgpf("node[{}]: set key tag: [{}]~~~{}~~~", m_curr->node_id, tag.len, tag);
        _enable_(KEYTAG);
        m_tree->m_key_tag[m_curr->node_id] = tag;
    }
    void set_val_tag(csubstr tag) noexcept
    {
        _c4dbgpf("node[{}]: set val tag: [{}]~~~{}~~~", m_curr->node_id, tag.len, tag);
        _enable_(VALTAG);
        m_tree->m_val_tag[m_curr->node_id] = tag;
    }

    /** @} */

public:

    /** @name YAML directive events */
    /** @{ */

    void add_directive(csubstr directive)
    {
        _c4dbgpf("% directive! {}", directive);
        _RYML_CB_ASSERT(m_stack.m_callbacks, directive.begins_with('%'));
        if(directive.begins_with("%TAG"))
        {
            // TODO do not use directives in the tree
            _RYML_CB_CHECK(m_stack.m_callbacks, m_tree->add_tag_directive(directive));
        }
        else if(directive.begins_with("%YAML"))
        {
            _c4dbgpf("%YAML directive! ignoring...: {}", directive);
        }
        else
        {
            _c4dbgpf("% directive unknown! ignoring...: {}", directive);
        }
    }

    /** @} */

public:

    /** @name arena functions */
    /** @{ */

    substr alloc_arena(size_t len)
    {
        csubstr prev = m_tree->arena();
        substr out = m_tree->alloc_arena(len);
        substr curr = m_tree->arena();
        if(curr.str != prev.str)
            _stack_relocate_to_new_arena(prev, curr);
        return out;
    }

    substr alloc_arena(size_t len, substr *relocated)
    {
        csubstr prev = m_tree->arena();
        if(!prev.is_super(*relocated))
            return alloc_arena(len);
        substr out = alloc_arena(len);
        substr curr = m_tree->arena();
        if(curr.str != prev.str)
            *relocated = _stack_relocate_to_new_arena(*relocated, prev, curr);
        return out;
    }

    /** @} */

public:

    /** @cond dev */
    void _reset_parser_state(state* st, id_type parse_root, id_type node)
    {
        _set_state_(st, node);
        const NodeType type = m_tree->type(node);
        #ifdef RYML_DBG
        char flagbuf[80];
        #endif
        _c4dbgpf("resetting state: initial flags={}", detail::_parser_flags_to_str(flagbuf, st->flags));
        if(type == NOTYPE)
        {
            _c4dbgpf("node[{}] is notype", node);
            if(m_tree->is_root(parse_root))
            {
                _c4dbgpf("node[{}] is root", node);
                st->flags |= RUNK|RTOP;
            }
            else
            {
                _c4dbgpf("node[{}] is not root. setting USTY", node);
                st->flags |= USTY;
            }
        }
        else if(type.is_map())
        {
            _c4dbgpf("node[{}] is map", node);
            st->flags |= RMAP|USTY;
        }
        else if(type.is_seq())
        {
            _c4dbgpf("node[{}] is map", node);
            st->flags |= RSEQ|USTY;
        }
        else if(type.has_key())
        {
            _c4dbgpf("node[{}] has key. setting USTY", node);
            st->flags |= USTY;
        }
        else
        {
            _RYML_CB_ERR(m_stack.m_callbacks, "cannot append to node");
        }
        if(type.is_doc())
        {
            _c4dbgpf("node[{}] is doc", node);
            st->flags |= RDOC;
        }
        _c4dbgpf("resetting state: final flags={}", detail::_parser_flags_to_str(flagbuf, st->flags));
    }

    /** push a new parent, add a child to the new parent, and set the
     * child as the current node */
    void _push()
    {
        _stack_push();
        m_curr->node_id = m_tree->_append_child__unprotected(m_parent->node_id);
        _c4dbgpf("pushed! level={}. top is now node={} (parent={})", m_curr->level, m_curr->node_id, m_parent ? m_parent->node_id : NONE);
    }
    /** end the current scope */
    void _pop()
    {
        _remove_speculative_with_parent();
        _stack_pop();
    }

public:

    template<NodeType_e bits> C4_HOT C4_ALWAYS_INLINE void _enable__() noexcept
    {
        m_tree->m_type[m_curr->node_id] |= bits;
    }
    template<NodeType_e bits> C4_HOT C4_ALWAYS_INLINE void _disable__() noexcept
    {
        m_tree->m_type[m_curr->node_id] &= ~bits;
    }
    template<NodeType_e bits> C4_HOT C4_ALWAYS_INLINE bool _has_any__() const noexcept
    {
        return ((m_tree->m_type[m_curr->node_id].type & bits) != 0);
    }

public:

    C4_ALWAYS_INLINE void _set_state_(state *C4_RESTRICT s, id_type id) noexcept
    {
        s->node_id = id;
    }

    void _set_root_as_stream()
    {
        _c4dbgp("set root as stream");
        const id_type node = m_curr->node_id;
        const id_type root = m_tree->root_id();
        _RYML_CB_ASSERT(m_stack.m_callbacks, root == 0u);
        _RYML_CB_ASSERT(m_stack.m_callbacks, node == 0u);
        const bool hack = !m_tree->has_children(node) && !m_tree->is_val(node);
        if(hack)
            m_tree->m_type[node].add(VAL);
        m_tree->set_root_as_stream();
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->is_stream(root));
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->has_children(root));
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->is_doc(m_tree->first_child(root)));
        if(hack)
            m_tree->m_type[m_tree->first_child(root)].rem(VAL);
        _set_state_(m_curr, root);
    }

    void _remove_speculative()
    {
        _c4dbgp("remove speculative node");
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->size() > 0);
        const id_type last_added = m_tree->size() - 1;
        if(m_tree->has_parent(last_added))
            if(m_tree->m_type[last_added] == NOTYPE)
                m_tree->remove(last_added);
    }

    void _remove_speculative_with_parent()
    {
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->size() > 0);
        const id_type last_added = m_tree->size() - 1;
        _RYML_CB_ASSERT(m_stack.m_callbacks, m_tree->has_parent(last_added));
        if(m_tree->m_type[last_added] == NOTYPE)
        {
            _c4dbgpf("remove speculative node with parent. parent={} node={} parent(node)={}", m_parent->node_id, last_added, m_tree->parent(last_added));
            m_tree->remove(last_added);
        }
    }

    C4_ALWAYS_INLINE void _save_loc()
    {
        m_tree->m_val[m_curr->node_id].str = m_curr->line_contents.rem.str;
    }

#undef _enable_
#undef _disable_
#undef _has_any_

    /** @endcond */
};

/** @} */

} // namespace yml
} // namespace c4

C4_SUPPRESS_WARNING_MSVC_POP

#endif /* _C4_YML_EVENT_HANDLER_TREE_HPP_ */
