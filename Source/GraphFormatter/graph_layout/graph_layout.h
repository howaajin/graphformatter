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
    struct connected_graph_t;
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
            return vector2_t{x + other.x, y + other.y};
        }

        vector2_t operator-(const vector2_t& other) const
        {
            return vector2_t{x - other.x, y - other.y};
        }

        vector2_t operator*(float factor) const
        {
            return vector2_t{x * factor, y * factor};
        }
    };

    struct rect_t
    {
        float l = 0, t = 0, r = 0, b = 0;

        rect_t offset_by(vector2_t offset) const
        {
            return rect_t{l + offset.x, t + offset.y, r + offset.x, b + offset.y};
        }

        rect_t expand(vector2_t pos, vector2_t size) const
        {
            return expand(rect_t{pos.x, pos.y, pos.x + size.x, pos.y + size.y});
        }

        rect_t expand(const rect_t& other) const
        {
            auto rect = rect_t{l, t, r, b};
            if (rect.l > other.l) rect.l = other.l;
            if (rect.t > other.t) rect.t = other.t;
            if (rect.r < other.r) rect.r = other.r;
            if (rect.b < other.b) rect.b = other.b;
            return rect;
        }

        vector2_t size() const
        {
            return vector2_t{r - l, b - t};
        }
    };

    struct pin_t
    {
        pin_type_t type = pin_type_t::in;
        vector2_t offset{0, 0};
        node_t* owner = nullptr;
        int index_in_layer = -1;
        pin_t* copy_from = nullptr;
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
        bool is_crossing(const edge_t* other) const;
        bool is_inner_segment() const;
    };

    struct node_t
    {
        std::string name;
        bool is_dummy_node = false;
        graph_t* graph = nullptr;
        void* user_ptr = nullptr;
        int rank{-1};
        float layer_order = -1.0f;
        // Is the node belongs to the head component?
        bool belongs_to_head = false;
        // Is the node belongs to the tail component?
        bool belongs_to_tail = false;
        vector2_t position{0, 0};
        vector2_t size{50, 50};
        std::vector<edge_t*> in_edges{};
        std::vector<edge_t*> out_edges{};
        std::vector<pin_t*> in_pins{};
        std::vector<pin_t*> out_pins{};
        bool is_descendant_of(node_t* node) const;
        void set_position(vector2_t p);
        pin_t* add_pin(pin_type_t type);
        std::vector<edge_t*> get_edges_linked_to_layer(const std::vector<node_t*>& layer, bool is_in) const;
        bool is_crossing_inner_segment(const std::vector<node_t*>& lower_layer, const std::vector<node_t*>& upper_layer) const;
        float get_barycenter_in_layer(const std::vector<node_t*>& layer, bool is_in) const;
        std::set<node_t*> get_direct_connected_nodes(const std::function<bool(edge_t*)>& filter) const;
        std::set<node_t*> get_out_nodes() const;
        std::set<node_t*> get_in_nodes() const;
        node_t* get_median_upper() const;
        std::vector<node_t*> get_uppers() const;
        std::vector<node_t*> get_lowers() const;
        float get_max_weight(bool is_in) const;
        float get_max_weight_to_node(const node_t* node, bool is_in) const;
        float get_linked_position_to_node(const node_t* node, bool is_in, bool is_horizontal_dir = true) const;
        void update_pins_offset();
        void set_sub_graph(connected_graph_t* g);

        node_t* clone() const;
        ~node_t();
    };

    struct tree_t
    {
        std::set<edge_t*> tree_edges;
        std::set<edge_t*> non_tree_edges;
        std::set<node_t*> nodes;
        edge_t* find_min_incident_edge(node_t** incident_node);
        void tighten() const;
        tree_t tight_sub_tree() const;
        edge_t* leave_edge() const;
        edge_t* enter_edge(edge_t* edge);
        void exchange(edge_t* e, edge_t* f);
        void calculate_cut_values();
        void update_non_tree_edges(const std::set<edge_t*>& all_edges);

    private:
        void reset_head_or_tail() const;
        void split_to_head_tail(edge_t* edge);
        void mark_head_or_tail(node_t* n, edge_t* cut_edge, bool is_head);
        static void add_to_weights(const edge_t* edge, int& head_to_tail_weight, int& tail_to_head_weight);
    };

    enum class rank_slot_t { none, min, max, };

    struct graph_t
    {
        virtual void translate(vector2_t offset);
        virtual ~graph_t();
        void set_position(vector2_t position);
        virtual std::vector<pin_t*> get_pins() const { return {}; }
        virtual std::map<pin_t*, vector2_t> get_pins_offset() { return {}; }
        virtual std::map<node_t*, rect_t> get_bounds() { return {}; }
        virtual void arrange() { }
        virtual graph_t* clone() const;

        node_t* add_node(graph_t* sub_graph = nullptr);
        node_t* add_node(const std::string& name, graph_t* sub_graph = nullptr);
        void remove_node(node_t* node);
        edge_t* add_edge(pin_t* tail, pin_t* head);
        void remove_edge(const edge_t* edge);
        void remove_edge(pin_t* tail, pin_t* head);
        void invert_edge(edge_t* edge) const;

        rect_t bound{0, 0, 0, 0};
        rect_t border{0, 0, 0, 0};
        std::vector<node_t*> nodes;
        std::map<std::pair<pin_t*, pin_t*>, edge_t*> edges;
        std::map<node_t*, graph_t*> sub_graphs;
        vector2_t spacing = {80, 80};
        bool is_vertical_layout = false;
    };

    struct disconnected_graph_t : public graph_t
    {
        void add_graph(graph_t* graph);
        ~disconnected_graph_t() override;
        void translate(vector2_t offset) override;
        std::vector<pin_t*> get_pins() const override;
        std::map<pin_t*, vector2_t> get_pins_offset() override;
        std::map<node_t*, rect_t> get_bounds() override;
        void arrange() override;

    private:
        std::vector<graph_t*> connected_graphs;
    };

    struct connected_graph_t : public graph_t
    {
        size_t max_iterations = 24;
        node_t* min_ranking_node = nullptr;
        node_t* max_ranking_node = nullptr;
        std::vector<std::vector<node_t*>> layers;

        graph_t* clone() const override;
        connected_graph_t* clone(std::map<node_t*, node_t*>& nodes_map, std::map<pin_t*, pin_t*>& pins_map, std::map<edge_t*, edge_t*>& edges_map,
                                 std::map<node_t*, node_t*>& nodes_map_inv, std::map<pin_t*, pin_t*>& pins_map_inv, std::map<edge_t*, edge_t*>& edges_map_inv) const;

        void set_node_in_rank_slot(node_t* node, rank_slot_t rank_slot);

        void merge_edges();

        std::vector<node_t*> get_source_nodes() const;
        std::vector<node_t*> get_sink_nodes() const;

        void acyclic() const;
        void rank() const;
        void add_dummy_nodes(tree_t* feasible_tree);
        void assign_layers();
        void ordering();

        void translate(vector2_t offset) override;
        void arrange() override;
        std::vector<pin_t*> get_pins() const override;
        std::map<pin_t*, vector2_t> get_pins_offset() override;
        std::map<node_t*, rect_t> get_bounds() override;

        void assign_coordinate();
        std::vector<rect_t> get_layers_bound() const;
        void sort_layers(std::vector<std::vector<node_t*>>& layer_vec, bool is_down) const;
        tree_t feasible_tree() const;
        std::string generate_test_code();

        static void calculate_pins_index_in_layer(const std::vector<node_t*>& layer);
        static std::vector<edge_t*> get_edges_between_two_layers(const std::vector<node_t*>& lower, const std::vector<node_t*>& upper, const node_t* excluded_node = nullptr);
        static size_t crossing(const std::vector<std::vector<node_t*>>& order, bool calculate_pins_index);
        static void test();

    private:
        void init_rank() const;
        void normalize() const;
        tree_t tight_tree() const;
        std::vector<node_t*> get_nodes_without_unscanned_in_edges(const std::set<node_t*>& visited, const std::set<edge_t*>& scanned_set) const;
    };
}
