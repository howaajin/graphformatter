/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Howaajin. All rights reserved.
 *  Licensed under the MIT License. See License in the project root for license information.
 *--------------------------------------------------------------------------------------------*/

#pragma once

#include <utility>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <functional>
#include <iostream>

namespace graph_layout
{
    struct graph_t;
    struct node_t;
    struct vector2_t;

    enum class pin_type_t
    {
        in,
        out,
    };

    struct vector2_t
    {
        float x, y;

        vector2_t operator+(const vector2_t& other) const
        {
            return vector2_t{this->x + other.x, this->y + other.y};
        }

        vector2_t operator-(const vector2_t& other) const
        {
            return vector2_t{this->x - other.x, this->y - other.y};
        }
    };

    struct rect_t
    {
        float l, t, r, b;

        void offset_by(vector2_t offset)
        {
            l += offset.x;
            r += offset.x;
            t += offset.y;
            b += offset.y;
        }
    };

    struct pin_t
    {
        pin_type_t type = pin_type_t::in;
        vector2_t offset{0, 0};
        node_t* owner = nullptr;
    };

    struct edge_t
    {
        pin_t* tail = nullptr;
        pin_t* head = nullptr;
        int weight = 1;
        int min_length = 1;
        int cut_value = 0;
        bool is_inverted = false;
        int length() const;
        int slack() const;
    };

    struct node_t
    {
        std::string name;
        int rank{-1};
        // Is the node belongs to the head component?
        bool belongs_to_head = false;
        // Is the node belongs to the tail component?
        bool belongs_to_tail = false;
        graph_t* graph = nullptr;
        vector2_t position{0, 0};
        std::vector<edge_t*> in_edges;
        std::vector<edge_t*> out_edges;
        std::vector<pin_t*> pins;
        bool is_descendant_of(node_t* node) const;
        void set_position(vector2_t p);
        pin_t* add_pin(pin_type_t type);
        std::vector<node_t*> get_direct_connected_nodes(std::function<bool(edge_t*)> filter) const;
        ~node_t();
        node_t* clone() const;
    };

    struct graph_t
    {
        rect_t bound{0, 0, 0, 0};
        std::vector<node_t*> nodes;
        std::set<node_t*> min_ranking_set;
        std::set<node_t*> max_ranking_set;
        node_t* min_ranking_dummy_node = nullptr;
        node_t* max_ranking_dummy_node = nullptr;
        std::map<std::pair<pin_t*, pin_t*>, edge_t*> edges;
        std::vector<std::vector<node_t*>> layers;
        std::map<node_t*, graph_t*> sub_graphs;

        graph_t* clone() const;
        graph_t* clone(std::unordered_map<node_t*, node_t*>& nodes_map, std::unordered_map<pin_t*, pin_t*>& pins_map, std::unordered_map<edge_t*, edge_t*>& edges_map) const;
        ~graph_t();

        node_t* add_node(graph_t* sub_graph = nullptr);
        node_t* add_node(const std::string& name, graph_t* sub_graph = nullptr);
        void remove_node(node_t* node);

        edge_t* add_edge(pin_t* tail, pin_t* head);
        void remove_edge(const edge_t* edge);
        void remove_edge(pin_t* tail, pin_t* head);
        void invert_edge(edge_t* edge) const;

        std::vector<pin_t*> get_pins() const;
        std::vector<node_t*> get_source_nodes() const;
        std::vector<node_t*> get_sink_nodes() const;

        void translate(vector2_t offset);
        void set_position(vector2_t position);
        void acyclic();
        void rank();
        void feasible_tree(std::set<edge_t*>& non_tree_edges);
        static void test();

    private:
        void init_rank();
        void calculate_cut_values(std::set<edge_t*>& non_tree_edges);
        edge_t* leave_edge(const std::set<edge_t*>& non_tree_edges);
        edge_t* enter_edge(edge_t* edge, std::set<edge_t*>& non_tree_edges);
        void exchange(edge_t* tree_edge, edge_t* non_tree_edge, std::set<edge_t*>& non_tree_edges);
        void normalize();
        static void mark_head_or_tail(node_t* n, bool is_head, bool reset, const std::set<edge_t*>& non_tree_edges);
        static void add_to_weights(const edge_t* edge, int& head_to_tail_weight, int& tail_to_head_weight);
        size_t tight_tree(std::set<node_t*>& tree_nodes, std::set<edge_t*>& non_tree_edges) const;
        static edge_t* find_min_incident_edge(const std::set<node_t*>& tree, const std::set<edge_t*>& non_tree_edges, node_t** incident_node);
        std::vector<node_t*> get_nodes_without_unscanned_in_edges(const std::set<node_t*>& visited, const std::set<edge_t*>& scanned_set) const;
    };
}
