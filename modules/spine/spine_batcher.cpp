/******************************************************************************
 * Spine Runtimes Software License v2.5
 *
 * Copyright (c) 2013-2016, Esoteric Software
 * All rights reserved.
 *
 * You are granted a perpetual, non-exclusive, non-sublicensable, and
 * non-transferable license to use, install, execute, and perform the Spine
 * Runtimes software and derivative works solely for personal or internal
 * use. Without the written permission of Esoteric Software (see Section 2 of
 * the Spine Software License Agreement), you may not (a) modify, translate,
 * adapt, or develop new applications using the Spine Runtimes or otherwise
 * create derivative works or improvements of the Spine Runtimes or (b) remove,
 * delete, alter, or obscure any trademarks or any copyright, trademark, patent,
 * or other intellectual property or proprietary rights notices on or in the
 * Software, including any copy thereof. Redistributions in binary or source
 * form must include this license and terms.
 *
 * THIS SOFTWARE IS PROVIDED BY ESOTERIC SOFTWARE "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL ESOTERIC SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, BUSINESS INTERRUPTION, OR LOSS OF
 * USE, DATA, OR PROFITS) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/
#ifdef MODULE_SPINE_ENABLED
#include "spine_batcher.h"

#define BATCH_CAPACITY 1024

Vector< SpineBatcher::Elements* > *spine_elements_pool;
int spine_elements_pool_last = 0;

SpineBatcher::Elements* spine_get_element(){
	return memnew(SpineBatcher::Elements);
	if (spine_elements_pool==NULL){
		spine_elements_pool = memnew(Vector< SpineBatcher::Elements* >);
	}
	if (spine_elements_pool_last==0){
		for (int i=0; i<spine_elements_pool->size(); i++){
			spine_elements_pool->get(i)->pool_idx += 1024;
		}
		for (int i=0; i<1024; i++){
			SpineBatcher::Elements *elem = memnew(SpineBatcher::Elements);
			elem->pool_idx = 1023-i;
			spine_elements_pool->insert(0, elem);
		}
		spine_elements_pool_last = 1024;
	}
	spine_elements_pool_last--;
	return spine_elements_pool->get(spine_elements_pool_last);
	//SpineBatcher::Elements *elem = spine_elements_pool->get(spine_elements_pool_last);
	//elem->pool_idx = spine_elements_pool_last;
	//return elem;
}

void spine_resume_element(SpineBatcher::Elements *elem){
	return memdelete(elem);
	if (!spine_elements_pool->size()) return;
	int nidx = spine_elements_pool_last;
	spine_elements_pool_last = nidx+1;
	if (spine_elements_pool_last == spine_elements_pool->size()){
		for (int i=0; i<spine_elements_pool->size(); i++){
			memdelete(spine_elements_pool->get(i));
		}
		spine_elements_pool->resize(0);
		return;
	}
	elem->texture = Ref<Texture>();
	elem->vertices_count = 0;
	elem->indies_count = 0;

	if (elem->pool_idx == nidx){
		spine_elements_pool->get(0)->pool_idx = nidx;
		elem->pool_idx = 0;
		spine_elements_pool->set(nidx, spine_elements_pool->get(0));
		spine_elements_pool->set(0, elem);
		return;
	}

	SpineBatcher::Elements *used = spine_elements_pool->get(nidx);
	used->pool_idx = elem->pool_idx;
	elem->pool_idx = nidx;
	spine_elements_pool->set(used->pool_idx, used);
	spine_elements_pool->set(elem->pool_idx, elem);
}

SpineBatcher::SetBlendMode::SetBlendMode(int p_mode) {

	cmd = CMD_SET_BLEND_MODE;
	mode = p_mode;
}

void SpineBatcher::SetBlendMode::draw(RID ci) {

	// VisualServer::get_singleton()->canvas_item_add_set_blend_mode(ci, VS::MaterialBlendMode(mode));
}

SpineBatcher::Elements::Elements() {

	cmd = CMD_DRAW_ELEMENT;
	vertices = memnew_arr(Vector2, BATCH_CAPACITY);
	colors = memnew_arr(Color, BATCH_CAPACITY);
	uvs = memnew_arr(Vector2, BATCH_CAPACITY);
	indies = memnew_arr(int, BATCH_CAPACITY * 3);
	vertices_count = 0;
	indies_count = 0;
};

SpineBatcher::Elements::~Elements() {

	memdelete_arr(vertices);
	memdelete_arr(colors);
	memdelete_arr(uvs);
	memdelete_arr(indies);
}

void SpineBatcher::Elements::draw(RID ci) {

	Vector<int> p_indices;
	p_indices.resize(indies_count);
	memcpy(p_indices.ptrw(), indies, indies_count * sizeof(int) );

	Vector<Vector2> p_points;
	p_points.resize(vertices_count);
	memcpy(p_points.ptrw(), vertices, vertices_count * sizeof(Vector2));

	Vector<Color> p_colors;
	p_colors.resize(vertices_count);
	memcpy(p_colors.ptrw(), colors, vertices_count * sizeof(Color));

	Vector<Vector2> p_uvs;
	p_uvs.resize(vertices_count);
	memcpy(p_uvs.ptrw(), uvs, vertices_count * sizeof(Vector2));

	VisualServer::get_singleton()->canvas_item_add_triangle_array(ci,
		p_indices,
		p_points,
		p_colors,
		p_uvs,
		Vector<int>(),
		Vector<float>(),
		texture->get_rid(), -1, RID(),
		false, false
	);

}

void SpineBatcher::add(Ref<Texture> p_texture,
	const float* p_vertices, const float* p_uvs, int p_vertices_count,
	const unsigned short* p_indies, int p_indies_count,
	Color *p_color, bool flip_x, bool flip_y, int index_item) {

	if (p_texture != elements->texture
		|| elements->vertices_count + (p_vertices_count >> 1) > BATCH_CAPACITY
		|| elements->indies_count + p_indies_count > BATCH_CAPACITY * 3) {

		push_elements();
		elements->texture = p_texture;
	}

	for (int i = 0; i < p_indies_count; ++i, ++elements->indies_count)
		elements->indies[elements->indies_count] = p_indies[i] + elements->vertices_count;

	for (int i = 0; i < p_vertices_count; i += 2, ++elements->vertices_count) {

		elements->vertices[elements->vertices_count].x = flip_x ? -p_vertices[i] : p_vertices[i];
		elements->vertices[elements->vertices_count].y = flip_y ? p_vertices[i + 1] : -p_vertices[i + 1];
		elements->colors[elements->vertices_count] = *p_color;
		elements->uvs[elements->vertices_count].x = p_uvs[i] + index_item;
		elements->uvs[elements->vertices_count].y = p_uvs[i + 1];
	}
}

void SpineBatcher::add_set_blender_mode(bool p_mode) {

	push_elements();
	element_list.push_back(memnew(SetBlendMode(p_mode)));
}

void SpineBatcher::flush() {

	RID ci = owner->get_canvas_item();
	push_elements();

	for (List<Command *>::Element *E = element_list.front(); E; E = E->next()) {

		Command *e = E->get();
		e->draw(ci);
		drawed_list.push_back(e);
	}
	element_list.clear();
}

void SpineBatcher::push_elements() {

	if (elements->vertices_count <= 0 || elements->indies_count <= 0)
		return;

	element_list.push_back(elements);
	//elements = memnew(Elements);
	elements = spine_get_element();
}

void SpineBatcher::reset() {

	for (List<Command *>::Element *E = drawed_list.front(); E; E = E->next()) {

		Command *e = E->get();
		//memdelete(e);
		spine_resume_element((Elements*)e);
	}
	drawed_list.clear();
}

SpineBatcher::SpineBatcher(Node2D *owner) : owner(owner) {

	//elements = memnew(Elements);
	elements = spine_get_element();

}

SpineBatcher::~SpineBatcher() {

	for (List<Command *>::Element *E = element_list.front(); E; E = E->next()) {

		Command *e = E->get();
		//memdelete(e);
		spine_resume_element((Elements*)e);
	}
	element_list.clear();

	for (List<Command *>::Element *E = drawed_list.front(); E; E = E->next()) {

		Command *e = E->get();
		//memdelete(e);
		spine_resume_element((Elements*)e);
	}
	drawed_list.clear();

	//memdelete(elements);
	spine_resume_element((Elements*)elements);
}

#endif // MODULE_SPINE_ENABLED
