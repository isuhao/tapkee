/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Copyright (c) 2012, Sergey Lisitsyn
 */

#include "tapkee.hpp"
#include "defines.hpp"
#include "callbacks/eigen_callbacks.hpp"
#include "callbacks/precomputed_callbacks.hpp"
#include "utils/logging.hpp"

#include <algorithm>
#include <string>
#include <istream>
#include <fstream>
#include <vector>
#include <iterator>

using namespace Eigen;
using namespace std;

DenseMatrix read_data(const string& filename)
{
	ifstream ifs(filename.c_str());
	string str;
	vector< vector<DefaultScalarType> > input_data;
	while (!ifs.eof())
	{
		getline(ifs,str);
		if (str.size())
		{
			stringstream strstr(str);
			istream_iterator<DefaultScalarType> it(strstr);
			istream_iterator<DefaultScalarType> end;
			vector<DefaultScalarType> row(it, end);
			input_data.push_back(row);
		}
	}
	DenseMatrix fm(input_data[0].size(),input_data.size());
	for (int i=0; i<fm.rows(); i++)
	{
		for (int j=0; j<fm.cols(); j++)
			fm(i,j) = input_data[j][i];
	}
	return fm;
}

TAPKEE_METHOD parse_reduction_method(const char* str)
{
	if (!strcmp(str,"kltsa"))
		return KERNEL_LOCAL_TANGENT_SPACE_ALIGNMENT;
	if (!strcmp(str,"klle"))
		return KERNEL_LOCALLY_LINEAR_EMBEDDING;
	if (!strcmp(str,"mds"))
		return MULTIDIMENSIONAL_SCALING;
	if (!strcmp(str,"lmds"))
		return LANDMARK_MULTIDIMENSIONAL_SCALING;
	if (!strcmp(str,"isomap"))
		return ISOMAP;
	if (!strcmp(str,"diffusion_map"))
		return DIFFUSION_MAP;
	if (!strcmp(str,"kpca"))
		return KERNEL_PCA;
	if (!strcmp(str,"pca"))
		return PCA;
	if (!strcmp(str,"laplacian_eigenmaps"))
		return LAPLACIAN_EIGENMAPS;
	if (!strcmp(str,"lpp"))
		return LOCALITY_PRESERVING_PROJECTIONS;
	if (!strcmp(str,"npe"))
		return NEIGHBORHOOD_PRESERVING_EMBEDDING;
	if (!strcmp(str,"lltsa"))
		return LINEAR_LOCAL_TANGENT_SPACE_ALIGNMENT;
	if (!strcmp(str,"spe"))
		return STOCHASTIC_PROXIMITY_EMBEDDING;
	if (!strcmp(str,"lisomap"))
		return LANDMARK_ISOMAP;

	printf("Method %s is not supported (yet?)\n",str);
	exit(EXIT_FAILURE);
	return KERNEL_LOCALLY_LINEAR_EMBEDDING;
}

TAPKEE_NEIGHBORS_METHOD parse_neighbors_method(const char* str)
{
	if (!strcmp(str,"brute"))
		return BRUTE_FORCE;
	if (!strcmp(str,"covertree"))
		return COVER_TREE;

	printf("Method %s is not supported (yet?)\n",str);
	exit(EXIT_FAILURE);
	return BRUTE_FORCE;
}

TAPKEE_EIGEN_EMBEDDING_METHOD parse_eigen_method(const char* str)
{
	if (!strcmp(str,"arpack"))
		return ARPACK;
	if (!strcmp(str,"randomized"))
		return RANDOMIZED;
	if (!strcmp(str,"dense"))
		return EIGEN_DENSE_SELFADJOINT_SOLVER;

	printf("Method %s is not supported (yet?)\n",str);
	exit(EXIT_FAILURE);
	return RANDOMIZED;
}

template <class RandomAccessIterator, class PairwiseCallback>
DenseMatrix matrix_from_callback(RandomAccessIterator begin, RandomAccessIterator end, PairwiseCallback callback)
{
	DenseMatrix result((end-begin),(end-begin));
	for (RandomAccessIterator i_iter=begin; i_iter!=end; ++i_iter)
	{
		for (RandomAccessIterator j_iter=begin; j_iter!=end; ++j_iter)
		{
			result((i_iter-begin),(j_iter-begin)) = callback(*i_iter,*j_iter);
		}
	}
	return result;
}

int main(int argc, const char** argv)
{
	ParametersMap parameters;
	if (argc!=6)
	{
		printf("No parameters specified.\n");
		printf("Usage is [method] [neighbor_method] [eigen_method] "
				" [number of neighbors] [target dimension]\n");
		exit(EXIT_FAILURE);
	}
	else
	{
		parameters[REDUCTION_METHOD] = parse_reduction_method(argv[1]);
		parameters[NEIGHBORS_METHOD] = parse_neighbors_method(argv[2]);
		parameters[EIGEN_EMBEDDING_METHOD] = parse_eigen_method(argv[3]);
		parameters[NUMBER_OF_NEIGHBORS] = static_cast<unsigned int>(atoi(argv[4]));
		parameters[TARGET_DIMENSION] = static_cast<unsigned int>(atoi(argv[5]));
		// keep it static yet
		parameters[DIFFUSION_MAP_TIMESTEPS] = static_cast<unsigned int>(3);
		parameters[GAUSSIAN_KERNEL_WIDTH] = static_cast<DefaultScalarType>(1000.0);
		parameters[SPE_GLOBAL_STRATEGY] = static_cast<bool>(true);
		parameters[SPE_TOLERANCE] = static_cast<DefaultScalarType>(1e-5);
		parameters[SPE_NUM_UPDATES] = static_cast<unsigned int>(100);
		parameters[LANDMARK_RATIO] = static_cast<DefaultScalarType>(0.2);
		parameters[EIGENSHIFT] = static_cast<DefaultScalarType>(1e-9);
	}


	// Load data
	DenseMatrix input_data = read_data("input.dat");
	parameters[CURRENT_DIMENSION] = static_cast<unsigned int>(input_data.rows());
	
	std::stringstream ss;
	ss << "Data contains " << input_data.cols() << " feature vectors with dimension of " << input_data.rows();
	LoggingSingleton::instance().info(ss.str());
	
	vector<int> data_indices;
	for (int i=0; i<input_data.cols(); i++)
		data_indices.push_back(i);
	// Embed
	DenseMatrix embedding;
	
#ifdef USE_PRECOMPUTED
	DenseMatrix distance_matrix = 
		matrix_from_callback(data_indices.begin(),data_indices.end(),distance_callback(input_data));
	precomputed_distance_callback dcb(distance_matrix);
	DenseMatrix kernel_matrix = 
		matrix_from_callback(data_indices.begin(),data_indices.end(),kernel_callback(input_data));
	precomputed_kernel_callback kcb(kernel_matrix);
	feature_vector_callback fvcb(input_data);

	embedding = embed(data_indices.begin(),data_indices.end(),kcb,dcb,fvcb,parameters);
#else
	distance_callback dcb(input_data);
	kernel_callback kcb(input_data);
	feature_vector_callback fvcb(input_data);

	embedding = embed(data_indices.begin(),data_indices.end(),kcb,dcb,fvcb,parameters);
#endif
	// Save obtained data
	ofstream ofs("output.dat");
	ofs << embedding;
	ofs.close();
	return 0;
}
