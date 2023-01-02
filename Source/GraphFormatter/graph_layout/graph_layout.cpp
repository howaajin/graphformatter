/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#include "graph_layout.h"

#include <limits>
#include <memory>
#include <algorithm>

namespace graph_layout
{
    using namespace std;

    static void dfs(const node_t* node, set<node_t*>& visited_set, function<void(node_t*)> on_visit, function<void(edge_t*)> on_non_tree_edge_found)
    {
        for (auto e : node->out_edges)
        {
            auto n = e->head->owner;
            if (visited_set.find(n) == visited_set.end())
            {
                visited_set.insert(n);
                if (on_visit) on_visit(n);
                dfs(n, visited_set, on_visit, on_non_tree_edge_found);
            }
            else
            {
                if (on_non_tree_edge_found)on_non_tree_edge_found(e);
            }
        }
    }

    static void dfs_inv(const node_t* node, set<node_t*>& visited_set, function<void(node_t*)> on_visit, function<void(edge_t*)> on_non_tree_edge_found)
    {
        for (auto e : node->in_edges)
        {
            auto n = e->tail->owner;
            if (visited_set.find(n) == visited_set.end())
            {
                visited_set.insert(n);
                if (on_visit) on_visit(n);
                dfs_inv(n, visited_set, on_visit, on_non_tree_edge_found);
            }
            else
            {
                if (on_non_tree_edge_found)on_non_tree_edge_found(e);
            }
        }
    }

    int edge_t::length() const
    {
        return head->owner->rank - tail->owner->rank;
    }

    int edge_t::slack() const
    {
        return length() - min_length;
    }

    bool node_t::is_descendant_of(node_t* node) const
    {
        set<node_t*> visited_set;
        vector stack{node};
        while (!stack.empty())
        {
            auto n = stack.back();
            visited_set.insert(n);
            if (n == this)
            {
                return true;
            }
            stack.pop_back();
            for (auto e : n->out_edges)
            {
                if (visited_set.find(e->head->owner) == visited_set.end())
                {
                    stack.push_back(e->head->owner);
                }
            }
        }
        return false;
    }

    void node_t::set_position(vector2_t p)
    {
        vector2_t offset = p - position;
        position = p;
        if (graph)
        {
            graph->translate(offset);
        }
    }

    pin_t* node_t::add_pin(pin_type_t type)
    {
        auto pin = new pin_t{type};
        pin->owner = this;
        pins.push_back(pin);
        return pin;
    }

    vector<node_t*> node_t::get_direct_connected_nodes(function<bool(edge_t*)> filter) const
    {
        vector<node_t*> result;
        for (auto e : in_edges)
        {
            if (filter(e))
            {
                result.push_back(e->tail->owner);
            }
        }
        for (auto e : out_edges)
        {
            if (filter(e))
            {
                result.push_back(e->head->owner);
            }
        }
        return result;
    }

    node_t::~node_t()
    {
        for (auto pin : pins)
        {
            delete pin;
        }
        delete graph;
    }

    node_t* node_t::clone() const
    {
        auto new_node = new node_t();
        new_node->name = name;
        if (graph)
        {
            new_node->graph = graph->clone();
        }
        new_node->position = position;
        for (auto pin : pins)
        {
            auto new_pin = new pin_t();
            new_pin->type = pin->type;
            new_pin->offset = pin->offset;
            new_pin->owner = new_node;
            new_node->pins.push_back(new_pin);
        }
        return new_node;
    }

    graph_t* graph_t::clone() const
    {
        unordered_map<node_t*, node_t*> nodes_map;
        unordered_map<pin_t*, pin_t*> pins_map;
        unordered_map<edge_t*, edge_t*> edges_map;
        return clone(nodes_map, pins_map, edges_map);
    }

    graph_t* graph_t::clone(unordered_map<node_t*, node_t*>& nodes_map, unordered_map<pin_t*, pin_t*>& pins_map, unordered_map<edge_t*, edge_t*>& edges_map) const
    {
        auto cloned = new graph_t;
        cloned->bound = bound;
        unordered_map<pin_t*, pin_t*> pins_map_inv;
        for (auto n : nodes)
        {
            auto cloned_node = n->clone();
            nodes_map.insert(make_pair(cloned_node, n));
            for (size_t i = 0; i < n->pins.size(); i++)
            {
                pins_map_inv.insert(make_pair(n->pins[i], cloned_node->pins[i]));
                pins_map.insert(make_pair(cloned_node->pins[i], n->pins[i]));
            }
            cloned->nodes.push_back(cloned_node);
            if (cloned_node->graph)
            {
                cloned->sub_graphs.insert(make_pair(cloned_node, cloned_node->graph));
            }
        }
        for (auto e : edges)
        {
            auto tail_pin = pins_map_inv[e.second->tail];
            auto head_pin = pins_map_inv[e.second->head];
            auto edge = cloned->add_edge(tail_pin, head_pin);
            edges_map.insert(make_pair(edge, e.second));
        }
        return cloned;
    }

    graph_t::~graph_t()
    {
        for (auto [k, edge] : edges)
        {
            delete edge;
        }
        for (auto n : nodes)
        {
            delete n;
        }
    }

    node_t* graph_t::add_node(graph_t* sub_graph)
    {
        auto node = new node_t();
        node->graph = sub_graph;
        if (sub_graph)
        {
            sub_graphs.insert(make_pair(node, sub_graph));
        }
        nodes.push_back(node);
        return node;
    }

    node_t* graph_t::add_node(const string& name, graph_t* sub_graph)
    {
        auto node = add_node(sub_graph);
        node->name = name;
        return node;
    }

    void graph_t::remove_node(node_t* node)
    {
        if (node->graph)
        {
            sub_graphs.erase(node);
        }
        delete node;
    }

    edge_t* graph_t::add_edge(pin_t* tail, pin_t* head)
    {
        auto k = make_pair(tail, head);
        auto it = edges.find(k);
        if (it != edges.end())
        {
            return it->second;
        }
        auto edge = new edge_t{tail, head};
        edges.insert(make_pair(make_pair(tail, head), edge));
        tail->owner->out_edges.push_back(edge);
        head->owner->in_edges.push_back(edge);
        return edge;
    }

    void graph_t::remove_edge(const edge_t* edge)
    {
        auto key = make_pair(edge->tail, edge->head);
        edges.erase(key);
        auto& out_edges = edge->tail->owner->out_edges;
        auto& in_edges = edge->head->owner->in_edges;
        out_edges.erase(find(out_edges.begin(), out_edges.end(), edge));
        in_edges.erase(find(in_edges.begin(), in_edges.end(), edge));
        delete edge;
    }

    void graph_t::remove_edge(pin_t* tail, pin_t* head)
    {
        auto key = make_pair(tail, head);
        auto it = edges.find(key);
        if (it != edges.end())
        {
            remove_edge(it->second);
        }
    }

    void graph_t::invert_edge(edge_t* edge) const
    {
        if (edge->tail->type == pin_type_t::in)
        {
            auto& out_edges = edge->head->owner->out_edges;
            auto& in_edges = edge->tail->owner->in_edges;
            in_edges.erase(find(in_edges.begin(), in_edges.end(), edge));
            edge->tail->type = pin_type_t::out;
            edge->tail->owner->out_edges.push_back(edge);
            out_edges.erase(find(out_edges.begin(), out_edges.end(), edge));
            edge->head->type = pin_type_t::in;
            edge->head->owner->in_edges.push_back(edge);
        }
        else
        {
            auto& out_edges = edge->tail->owner->out_edges;
            auto& in_edges = edge->head->owner->in_edges;
            out_edges.erase(find(out_edges.begin(), out_edges.end(), edge));
            edge->tail->type = pin_type_t::in;
            edge->tail->owner->in_edges.push_back(edge);
            in_edges.erase(find(in_edges.begin(), in_edges.end(), edge));
            edge->head->type = pin_type_t::out;
            edge->head->owner->out_edges.push_back(edge);
        }
        edge->is_inverted = true;
    }

    vector<pin_t*> graph_t::get_pins() const
    {
        vector<pin_t*> pins;
        for (auto n : nodes)
        {
            pins.reserve(pins.size() + n->pins.size());
            pins.insert(pins.end(), n->pins.begin(), n->pins.end());
        }
        return pins;
    }

    vector<node_t*> graph_t::get_source_nodes() const
    {
        vector<node_t*> source_nodes;
        for (auto n : nodes)
        {
            if (n->in_edges.empty())
            {
                source_nodes.push_back(n);
            }
        }
        return source_nodes;
    }

    vector<node_t*> graph_t::get_sink_nodes() const
    {
        vector<node_t*> source_nodes;
        for (auto n : nodes)
        {
            if (n->out_edges.empty())
            {
                source_nodes.push_back(n);
            }
        }
        return source_nodes;
    }

    void graph_t::translate(vector2_t offset)
    {
        for (auto n : nodes)
        {
            n->set_position(n->position + offset);
        }
        bound.offset_by(offset);
    }

    void graph_t::set_position(vector2_t position)
    {
        translate(vector2_t{position.x - bound.l, position.y - bound.t});
    }

    void graph_t::acyclic()
    {
        if (nodes.empty())
        {
            return;
        }
        set<node_t*> visited_set;
        vector<edge_t*> non_tree_edges;
        unordered_map<node_t*, node_t*> nodes_map;
        unordered_map<pin_t*, pin_t*> pins_map;
        unordered_map<edge_t*, edge_t*> edges_map;
        graph_t* tree = clone(nodes_map, pins_map, edges_map);
        vector<node_t*> source_nodes = tree->get_source_nodes();
        if (!source_nodes.empty())
        {
            for (auto n : source_nodes)
            {
                visited_set.insert(n);
                dfs(n, visited_set, nullptr, [&non_tree_edges](edge_t* e)
                {
                    non_tree_edges.push_back(e);
                });
            }
        }
        else
        {
            vector<node_t*> sink_nodes = tree->get_sink_nodes();
            if (!sink_nodes.empty())
            {
                for (auto n : sink_nodes)
                {
                    visited_set.insert(n);
                    dfs_inv(n, visited_set, nullptr, [&non_tree_edges](edge_t* e)
                    {
                        non_tree_edges.push_back(e);
                    });
                }
            }
            else
            {
                visited_set.insert(tree->nodes[0]);
                dfs(tree->nodes[0], visited_set, nullptr, [&non_tree_edges](edge_t* e)
                {
                    non_tree_edges.push_back(e);
                });
            }
        }

        vector<pair<node_t*, node_t*>> node_pairs;
        vector<edge_t*> original_non_tree_edges;
        for (auto e : non_tree_edges)
        {
            auto tail_node = e->tail->owner;
            auto head_node = e->head->owner;
            original_non_tree_edges.push_back(edges_map[e]);
            node_pairs.emplace_back(tail_node, head_node);
            tree->remove_edge(e);
        }
        for (size_t i = 0; i < node_pairs.size(); i++)
        {
            auto p = node_pairs[i];
            if (p.first->is_descendant_of(p.second))
            {
                invert_edge(original_non_tree_edges[i]);
                printf("invert edge: %s->%s\n", original_non_tree_edges[i]->tail->owner->name.c_str(), original_non_tree_edges[i]->head->owner->name.c_str());
            }
        }
    }

    void graph_t::feasible_tree(set<edge_t*>& non_tree_edges)
    {
        init_rank();
        set<node_t*> tree_nodes;
        while (tight_tree(tree_nodes, non_tree_edges) < nodes.size())
        {
            node_t* incident_node;
            edge_t* e = find_min_incident_edge(tree_nodes, non_tree_edges, &incident_node);
            auto delta = e->slack();
            if (e->head->owner == incident_node)
            {
                delta = -delta;
            }
            for (auto n : tree_nodes)
            {
                n->rank = n->rank + delta;
            }
            tree_nodes.clear();
            non_tree_edges.clear();
        }
    }

    void graph_t::init_rank()
    {
        set<edge_t*> scanned_set;
        int ranking = 0;
        set<node_t*> visited_set;
        while (visited_set.size() != nodes.size())
        {
            auto queue = get_nodes_without_unscanned_in_edges(visited_set, scanned_set);
            while (!queue.empty())
            {
                auto n = queue.back();
                n->rank = ranking;
                visited_set.insert(n);
                for (auto e : n->out_edges)
                {
                    scanned_set.insert(e);
                }
                queue.pop_back();
            }
            ranking++;
        }
    }

    void graph_t::init_cut_values(set<edge_t*>& non_tree_edges)
    {
        set<node_t*> visited_nodes;
        for (auto& [fst, edge] : edges)
        {
            if (non_tree_edges.find(edge) != non_tree_edges.end())
            {
                continue;
            }
            non_tree_edges.insert(edge);

            auto n = edge->tail->owner;
            mark_head_or_tail(n, false, true, non_tree_edges);
            n = edge->head->owner;
            mark_head_or_tail(n, true, false, non_tree_edges);
            non_tree_edges.erase(edge);
            int head_to_tail_weight = 0;
            int tail_to_head_weight = 0;
            for (auto& [fst2, edge2] : edges)
            {
                if (edge2 == edge)
                {
                    continue;
                }
                add_to_weights(edge2, head_to_tail_weight, tail_to_head_weight);
            }
            edge->cut_value = edge->weight + tail_to_head_weight - head_to_tail_weight;
        }
    }

    void graph_t::mark_head_or_tail(node_t* n, bool is_head, bool reset, const std::set<edge_t*>& non_tree_edges)
    {
        set<node_t*> visited_nodes;
        vector<node_t*> stack;
        stack.push_back(n);
        while (!stack.empty())
        {
            auto node = stack.back();
            stack.pop_back();
            visited_nodes.insert(node);
            if (is_head)
            {
                node->belongs_to_head = true;
                if (reset) node->belongs_to_tail = false;
            }
            else
            {
                node->belongs_to_tail = true;
                if (reset) node->belongs_to_head = false;
            }
            auto direct_connected_nodes = node->get_direct_connected_nodes([&non_tree_edges](edge_t* e)
            {
                if (non_tree_edges.find(e) != non_tree_edges.end())
                {
                    return false;
                }
                return true;
            });
            for (auto direct_connected : direct_connected_nodes)
            {
                if (visited_nodes.find(direct_connected) == visited_nodes.end())
                {
                    stack.push_back(direct_connected);
                }
            }
        }
    }

    void graph_t::add_to_weights(const edge_t* edge, int& head_to_tail_weight, int& tail_to_head_weight)
    {
        auto tail = edge->tail->owner;
        auto head = edge->head->owner;
        if (tail->belongs_to_tail && head->belongs_to_head)
        {
            tail_to_head_weight += edge->weight;
        }
        if (tail->belongs_to_head && head->belongs_to_tail)
        {
            head_to_tail_weight += edge->weight;
        }
    }

    size_t graph_t::tight_tree(std::set<node_t*>& tree_nodes, std::set<edge_t*>& non_tree_edges) const
    {
        vector<node_t*> stack;
        set<node_t*> non_tree_nodes;
        int min_slack = std::numeric_limits<int>::max();
        if (min_ranking_dummy_node)
        {
            stack.push_back(min_ranking_dummy_node);
        }
        else
        {
            if (max_ranking_dummy_node)
            {
                stack.push_back(max_ranking_dummy_node);
            }
            else
            {
                stack.push_back(nodes[0]);
            }
        }
        while (!stack.empty())
        {
            auto n = stack.back();
            tree_nodes.insert(n);
            stack.pop_back();
            auto connected_nodes = n->get_direct_connected_nodes([&min_slack, n ,&non_tree_edges, &non_tree_nodes](edge_t* e)
            {
                if (e->slack() != 0)
                {
                    non_tree_edges.insert(e);
                    if (e->slack() < min_slack)
                    {
                        min_slack = e->slack();
                    }
                    return false;
                }
                return true;
            });
            for (auto connected_node : connected_nodes)
            {
                if (tree_nodes.find(connected_node) == tree_nodes.end())
                {
                    stack.push_back(connected_node);
                }
            }
        }
        return tree_nodes.size();
    }

    edge_t* graph_t::find_min_incident_edge(const std::set<node_t*>& tree, const std::set<edge_t*>& non_tree_edges, node_t** incident_node)
    {
        edge_t* min_slack_edge = nullptr;
        int slack = std::numeric_limits<int>::max();
        *incident_node = nullptr;
        for (auto e : non_tree_edges)
        {
            bool head_is_tree_node = tree.find(e->head->owner) != tree.end();
            bool tail_is_tree_node = tree.find(e->tail->owner) != tree.end();
            if ((head_is_tree_node && !tail_is_tree_node) || (!head_is_tree_node && tail_is_tree_node))
            {
                if (e->slack() < slack)
                {
                    min_slack_edge = e;
                    slack = e->slack();
                    *incident_node = head_is_tree_node ? e->head->owner : e->tail->owner;
                }
            }
        }
        return min_slack_edge;
    }

    vector<node_t*> graph_t::get_nodes_without_unscanned_in_edges(const set<node_t*>& visited, const set<edge_t*>& scanned_set) const
    {
        vector<node_t*> result;
        for (auto n : nodes)
        {
            if (visited.find(n) != visited.end())
            {
                continue;
            }
            bool is_all_scanned = true;
            for (auto e : n->in_edges)
            {
                if (scanned_set.find(e) == scanned_set.end())
                {
                    is_all_scanned = false;
                    break;
                }
            }
            if (is_all_scanned)
            {
                result.push_back(n);
            }
        }
        return result;
    }

    void graph_t::test()
    {
        graph_t g;
        auto node_a = g.add_node("VertexNormalWs");
        auto node_b = g.add_node("Camera Vector");
        auto node_c = g.add_node("Dot");
        auto node_d = g.add_node("Clamp");
        auto node_e = g.add_node("Power");
        auto node_f = g.add_node("Add");
        auto node_g = g.add_node("Lerp");
        auto node_h = g.add_node("Multiply");
        auto node_i = g.add_node("Add_i");
        auto node_j = g.add_node("GizmoColor");
        auto node_k = g.add_node("Multiply_k");
        auto node_l = g.add_node("GizmoMaterial");

        auto pin_a_out = node_a->add_pin(pin_type_t::out);
        auto pin_b_out = node_b->add_pin(pin_type_t::out);
        auto pin_c_in_1 = node_c->add_pin(pin_type_t::in);
        auto pin_c_in_2 = node_c->add_pin(pin_type_t::in);
        auto pin_c_out = node_c->add_pin(pin_type_t::out);
        auto pin_d_in = node_d->add_pin(pin_type_t::in);
        auto pin_d_out = node_d->add_pin(pin_type_t::out);
        auto pin_e_in = node_e->add_pin(pin_type_t::in);
        auto pin_e_out = node_e->add_pin(pin_type_t::out);
        auto pin_f_in = node_f->add_pin(pin_type_t::in);
        auto pin_g_in = node_g->add_pin(pin_type_t::in);
        auto pin_g_out = node_g->add_pin(pin_type_t::out);
        auto pin_h_in = node_h->add_pin(pin_type_t::in);
        auto pin_h_out = node_h->add_pin(pin_type_t::out);
        auto pin_i_in_1 = node_i->add_pin(pin_type_t::in);
        auto pin_i_in_2 = node_i->add_pin(pin_type_t::in);
        auto pin_i_out = node_i->add_pin(pin_type_t::out);
        auto pin_j_out = node_j->add_pin(pin_type_t::out);
        auto pin_k_in_1 = node_k->add_pin(pin_type_t::in);
        auto pin_k_in_2 = node_k->add_pin(pin_type_t::in);
        auto pin_k_out = node_k->add_pin(pin_type_t::out);
        auto pin_l_in = node_l->add_pin(pin_type_t::in);

        g.add_edge(pin_a_out, pin_c_in_1);
        g.add_edge(pin_b_out, pin_c_in_2);
        g.add_edge(pin_c_out, pin_d_in);
        g.add_edge(pin_d_out, pin_e_in);
        g.add_edge(pin_d_out, pin_f_in);
        g.add_edge(pin_d_out, pin_g_in);
        g.add_edge(pin_e_out, pin_h_in);
        g.add_edge(pin_g_out, pin_i_in_2);
        g.add_edge(pin_h_out, pin_i_in_1);
        g.add_edge(pin_i_out, pin_k_in_1);
        g.add_edge(pin_j_out, pin_k_in_2);
        g.add_edge(pin_k_out, pin_l_in);

        g.acyclic();
        set<edge_t*> non_tree_edges;
        g.feasible_tree(non_tree_edges);
    }
}
