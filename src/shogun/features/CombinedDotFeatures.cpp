/*
 * This software is distributed under BSD 3-clause license (see LICENSE file).
 *
 * Authors: Soeren Sonnenburg, Evangelos Anagnostopoulos, Evan Shelhamer,
 *          Sergey Lisitsyn, Bjoern Esser
 */

#include <shogun/features/CombinedDotFeatures.h>
#include <shogun/io/SGIO.h>
#include <shogun/mathematics/Math.h>

#include <vector>

using namespace shogun;

constexpr float64_t CCombinedDotFeatures::initial_weight = 1.0;

CCombinedDotFeatures::CCombinedDotFeatures() : CDotFeatures()
{
	init();


	update_dim_feature_space_and_num_vec();
}

CCombinedDotFeatures::CCombinedDotFeatures(const CCombinedDotFeatures& orig)
    : CDotFeatures(orig), feature_array(orig.feature_array),
      feature_weights(orig.feature_weights), num_vectors(orig.num_vectors),
      num_dimensions(orig.num_dimensions)
{
	register_params();
	update_dim_feature_space_and_num_vec();
}

CFeatures* CCombinedDotFeatures::duplicate() const
{
	return new CCombinedDotFeatures(*this);
}

CCombinedDotFeatures::~CCombinedDotFeatures()
{
	SG_UNREF(feature_array);
}

void CCombinedDotFeatures::list_feature_objs()
{
	SG_INFO("BEGIN COMBINED DOTFEATURES LIST (%d, %d) - ", num_vectors, num_dimensions)
	this->list_feature_obj();

	for (index_t f_idx=0; f_idx<get_num_feature_obj(); f_idx++)
	{
		CDotFeatures* f = get_feature_obj(f_idx);
		f->list_feature_obj();
		SG_UNREF(f);
	}

	SG_INFO("END COMBINED DOTFEATURES LIST (%d, %d) - ", num_vectors, num_dimensions)
	this->list_feature_obj();
}

void CCombinedDotFeatures::update_dim_feature_space_and_num_vec()
{
	int32_t dim=0;
	int32_t vec=-1;

	for (index_t f_idx=0; f_idx<get_num_feature_obj(); f_idx++)
	{
		CDotFeatures* f = get_feature_obj(f_idx);
		dim+= f->get_dim_feature_space();
		if (vec==-1)
			vec=f->get_num_vectors();
		else if (vec != f->get_num_vectors())
		{
			f->list_feature_obj();
			SG_ERROR("Number of vectors (%d) mismatches in above feature obj (%d)\n", vec, f->get_num_vectors())
		}

		SG_UNREF(f);
	}

	num_dimensions=dim;
	num_vectors=vec;
	SG_DEBUG("vecs=%d, dims=%d\n", num_vectors, num_dimensions)
}

float64_t CCombinedDotFeatures::dot(int32_t vec_idx1, CDotFeatures* df, int32_t vec_idx2) const
{
	float64_t result=0;

	ASSERT(df)
	ASSERT(df->get_feature_type() == get_feature_type())
	ASSERT(df->get_feature_class() == get_feature_class())
	CCombinedDotFeatures* cf = (CCombinedDotFeatures*) df;

	// check that both have same number of feature objects inside
	ASSERT(get_num_feature_obj()==cf->get_num_feature_obj())

	for (index_t f_idx=0; f_idx<get_num_feature_obj(); f_idx++)
	{
		CDotFeatures* f1 = get_feature_obj(f_idx);
		CDotFeatures* f2 = cf->get_feature_obj(f_idx);

		ASSERT(f1)
		ASSERT(f2)

		result += f1->dot(vec_idx1, f2,vec_idx2) *
			get_subfeature_weight(f_idx) *
			cf->get_subfeature_weight(f_idx);

		SG_UNREF(f1);
		SG_UNREF(f2);
	}

	return result;
}

float64_t CCombinedDotFeatures::dense_dot(int32_t vec_idx1, const float64_t* vec2, int32_t vec2_len) const
{
	float64_t result=0;

	uint32_t offs=0;

	for (index_t f_idx=0; f_idx<get_num_feature_obj(); f_idx++)
	{
		CDotFeatures* f = get_feature_obj(f_idx);
		int32_t dim = f->get_dim_feature_space();
		result += f->dense_dot(vec_idx1, vec2+offs, dim)*get_subfeature_weight(f_idx);
		offs += dim;

		SG_UNREF(f);
	}

	return result;
}

void CCombinedDotFeatures::dense_dot_range(float64_t* output, int32_t start, int32_t stop, float64_t* alphas, float64_t* vec, int32_t dim, float64_t b) const
{
	ASSERT(stop > start)
	ASSERT(dim==num_dimensions)

	uint32_t offs=0;
	int32_t num=stop-start;
	SGVector<float64_t> tmp(num);
	std::fill(output, output + num, b);

	for (index_t f_idx=0; f_idx<get_num_feature_obj(); f_idx++)
	{
		CDotFeatures* f = get_feature_obj(f_idx);
		int32_t f_dim = f->get_dim_feature_space();

		f->dense_dot_range(
		    tmp.vector, start, stop, alphas, vec + offs, f_dim, 0);
		for (int32_t i=0; i<num; i++)
			output[i] += get_subfeature_weight(f_idx) * tmp[i];

		offs += f_dim;

		SG_UNREF(f);
	}
}

void CCombinedDotFeatures::dense_dot_range_subset(int32_t* sub_index, int32_t num, float64_t* output, float64_t* alphas, float64_t* vec, int32_t dim, float64_t b) const
{
	ASSERT(num > 0)
	ASSERT(dim==num_dimensions)

	uint32_t offs=0;

	SGVector<float64_t> tmp(num);
	std::fill(output, output + num, b);

	for (index_t f_idx=0; f_idx<get_num_feature_obj(); f_idx++)
	{
		CDotFeatures* f = get_feature_obj(f_idx);
		int32_t f_dim = f->get_dim_feature_space();

		f->dense_dot_range_subset(
			sub_index, num, tmp.vector, alphas, vec+offs, f_dim, 0);
		for (int32_t i=0; i<num; i++)
			output[i] += get_subfeature_weight(f_idx) * tmp[i];

		offs += f_dim;

		SG_UNREF(f);
	}
}

void CCombinedDotFeatures::add_to_dense_vec(float64_t alpha, int32_t vec_idx1, float64_t* vec2, int32_t vec2_len, bool abs_val) const
{
	uint32_t offs=0;

	for (index_t f_idx=0; f_idx<get_num_feature_obj(); f_idx++)
	{
		CDotFeatures* f = get_feature_obj(f_idx);
		int32_t dim = f->get_dim_feature_space();
		f->add_to_dense_vec(alpha*get_subfeature_weight(f_idx), vec_idx1, vec2+offs, dim, abs_val);
		offs += dim;

		SG_UNREF(f);
	}
}

void* CCombinedDotFeatures::get_feature_iterator(int32_t vector_index)
{
	combined_feature_iterator* it=SG_MALLOC(combined_feature_iterator, 1);

	it->f=get_feature_obj(0);
	it->iterator_idx = 0;
	it->iterator=it->f->get_feature_iterator(vector_index);
	it->vector_index=vector_index;
	return it;
}

bool CCombinedDotFeatures::get_next_feature(int32_t& index, float64_t& value, void* iterator)
{
	ASSERT(iterator)
	combined_feature_iterator* it = (combined_feature_iterator*) iterator;

	while (it->f)
	{
		if (it->f->get_next_feature(index, value, it->iterator))
		{
			value *= get_subfeature_weight(it->iterator_idx);
			return true;
		}

		if (++(it->iterator_idx) == get_num_feature_obj())
		{
			index = -1;
			break;
		}

		it->f->free_feature_iterator(it->iterator);
		SG_UNREF(it->f);
		it->f = get_feature_obj(it->iterator_idx);
		if (it->f)
			it->iterator=it->f->get_feature_iterator(it->vector_index);
		else
			it->iterator=NULL;
	}
	return false;
}

void CCombinedDotFeatures::free_feature_iterator(void* iterator)
{
	if (iterator)
	{
		combined_feature_iterator* it = (combined_feature_iterator*) iterator;
		if (it->iterator && it->f)
			it->f->free_feature_iterator(it->iterator);
		SG_UNREF(it->f);
		SG_FREE(it);
	}
}

CDotFeatures* CCombinedDotFeatures::get_feature_obj(int32_t idx) const
{
	return (CDotFeatures*) feature_array->get_element(idx);
}

bool CCombinedDotFeatures::insert_feature_obj(CDotFeatures* obj, int32_t idx)
{
	ASSERT(obj)
	bool result=feature_array->insert_element(obj, idx);
	auto iterator = feature_weights.begin() + idx;
	feature_weights.insert(iterator, initial_weight);
	update_dim_feature_space_and_num_vec();
	return result;
}

bool CCombinedDotFeatures::append_feature_obj(CDotFeatures* obj)
{
	ASSERT(obj)
	int n = get_num_feature_obj();
	feature_array->push_back(obj);
	feature_weights.push_back(initial_weight);
	update_dim_feature_space_and_num_vec();
	return n+1==get_num_feature_obj();
}

bool CCombinedDotFeatures::delete_feature_obj(int32_t idx)
{
	bool succesful_deletion = feature_array->delete_element(idx);
	if (succesful_deletion)
		update_dim_feature_space_and_num_vec();
	auto iterator = feature_weights.begin() + idx;
	feature_weights.erase(iterator);
	return succesful_deletion;
}

int32_t CCombinedDotFeatures::get_num_feature_obj() const
{
	return feature_array->get_num_elements();
}

int32_t CCombinedDotFeatures::get_nnz_features_for_vector(int32_t num) const
{
	int32_t result=0;

	for (index_t f_idx=0; f_idx<get_num_feature_obj(); f_idx++)
	{
		CDotFeatures* f = get_feature_obj(f_idx);
		result+=f->get_nnz_features_for_vector(num);
		SG_UNREF(f);
	}

	return result;
}

SGVector<float64_t> CCombinedDotFeatures::get_subfeature_weights() const
{
	int32_t num_weights = get_num_feature_obj();
	ASSERT(num_weights > 0)

	SGVector<float64_t> weights(num_weights);
	std::copy(feature_weights.begin(), feature_weights.end(), weights.vector);

	return weights;
}

void CCombinedDotFeatures::set_subfeature_weights(const SGVector<float64_t>& weights)
{
	ASSERT(weights.vlen==get_num_feature_obj())

	std::copy(
	    weights.vector, weights.vector + weights.vlen, feature_weights.begin());
}

float64_t CCombinedDotFeatures::get_subfeature_weight(index_t idx) const
{
	ASSERT(idx >= 0 && (size_t)idx < feature_weights.size())
	return feature_weights[idx];
}

void CCombinedDotFeatures::set_subfeature_weight(index_t idx, float64_t weight)
{
	REQUIRE(
	    idx >= 0 && (size_t)idx < feature_weights.size(),
	    "Index (%d) is out of bounds", idx);

	feature_weights[idx] = weight;
}

void CCombinedDotFeatures::init()
{
	feature_array=new CDynamicObjectArray();
	SG_REF(feature_array);
	register_params();
}

void CCombinedDotFeatures::register_params()
{
	SG_ADD(
	    &num_dimensions, "num_dimensions", "Total number of dimensions.");
	SG_ADD(
	    &num_vectors, "num_vectors", "Total number of vectors.");
	SG_ADD(&feature_array, "feature_array", "Feature array.");
	watch_param("feature_weights", &feature_weights);
}
