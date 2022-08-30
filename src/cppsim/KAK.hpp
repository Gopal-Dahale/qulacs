
#pragma once

#include <Eigen/Eigen>

#include "gate.hpp"
#include "gate_factory.hpp"
#include "general_quantum_operator.hpp"
#include "observable.hpp"
#include "gate_merge.hpp"
#include "state.hpp"

class KAK_data {
public:
    CPPCTYPE global_phase;
    QuantumGateMatrix* single_qubit_operations_before[2];
    double interaction_coefficients[3];
    QuantumGateMatrix* single_qubit_operations_after[2];
};

// clang-format off
Eigen::Matrix4cd KAK_MAGIC = (Eigen::Matrix4cd() <<  
                                1,  0,  0, 1i,
                                0, 1i,  1,  0,
                                0, 1i, -1,  0,
                                1,  0,  0,-1i)
                            .finished() *sqrt(0.5);


Eigen::Matrix4cd KAK_MAGIC_DAG = (Eigen::Matrix4cd() <<
                                1,  0,  0,  1,
                                0,-1i,-1i,  0,
                                0,  1, -1,  0,
                                -1i,0,  0, 1i)
                            .finished() *sqrt(0.5);

Eigen::Matrix4cd KAK_GAMMA = (Eigen::Matrix4cd() <<
                                1,  1,  1,  1,
                                1,  1, -1, -1,
                               -1,  1, -1,  1,
                                1, -1, -1,  1)
                            .finished() *0.25;
// clang-format on

std::pair<Eigen::Matrix<CPPCTYPE, 2, 2>, Eigen::Matrix<CPPCTYPE, 2, 2>>
so4_to_magic_su2s(Eigen::Matrix4cd mat) {
    Eigen::Matrix4cd ab = KAK_MAGIC * mat * KAK_MAGIC_DAG;
    //std::cout << "ab=" << ab << std::endl;
    Eigen::Matrix<CPPCTYPE, 2, 2> fa, fb;
    int max_r = 0, max_c = 0;
    for (int gr = 0; gr < 4; gr++) {
        for (int gc = 0; gc < 4; gc++) {
            if (abs(ab(max_r, max_c)) < abs(ab(gr, gc))) {
                max_r = gr;
                max_c = gc;
            }
        }
    }
    // ab[max_r][max_c] が　絶対値最大

    fa((max_r & 1), (max_c & 1)) = ab(max_r, max_c);
    fa((max_r & 1) ^ 1, (max_c & 1)) = ab(max_r ^ 1, max_c);
    fa((max_r & 1), (max_c & 1) ^ 1) = ab(max_r, max_c ^ 1);
    fa((max_r & 1) ^ 1, (max_c & 1) ^ 1) = ab(max_r ^ 1, max_c ^ 1);
    fb((max_r >> 1), (max_c >> 1)) = ab(max_r, max_c);
    fb((max_r >> 1) ^ 1, (max_c >> 1)) = ab(max_r ^ 2, max_c);
    fb((max_r >> 1), (max_c >> 1) ^ 1) = ab(max_r, max_c ^ 2);
    fb((max_r >> 1) ^ 1, (max_c >> 1) ^ 1) = ab(max_r ^ 2, max_c ^ 2);

    fa /= sqrt(fa(0, 0) * fa(1, 1) - fa(0, 1) * fa(1, 0));

    CPPCTYPE global = ab(max_r, max_c) / (fa((max_r & 1), (max_c & 1)) *
                                             fb((max_r >> 1), (max_c >> 1)));
    fb *= global;
    for (int gr = 0; gr < 4; gr++) {
        for (int gc = 0; gc < 4; gc++) {
            if(abs(fa((gr & 1), (gc & 1))*fb((gr >> 1), (gc >> 1))-ab(gr,gc))>0.001){
                std::cout<<"bug"<<std::endl;
            }
        }
    }
    return make_pair(fa, fb);
}

std::tuple<Eigen::Matrix4cd, Eigen::Matrix4cd>
bidiagonalize_real_matrix_pair_with_symmetric_products(
    Eigen::Matrix4d matA, Eigen::Matrix4d matB) {
    //両方が実数の場合の、同時特異値分解します
    Eigen::JacobiSVD<Eigen::Matrix4d> svd(
        matA, Eigen::ComputeThinU | Eigen::ComputeThinV);

    int rank = 4;
    while (rank > 0 && abs(svd.singularValues()(rank - 1, rank - 1)) < 1e-8) {
        rank--;
    }

    Eigen::Matrix4cd semi_corrected = svd.matrixU().transpose() * matB * svd.matrixV();
    
    std::cout << "semi=" << semi_corrected << std::endl;
    /*
        Eigen::EigenSolver<Eigen::Matrix4d> eigen_solver(semi_corrected);

        Eigen::Matrix4cd left_adjust = eigen_solver.eigenvectors();
        std::cout << "diag=" << eigen_solver.eigenvalues() << std::endl;
        std::cout << "left_ad=" << eigen_solver.eigenvectors() << std::endl;
        auto right_adjust = left_adjust.transpose();

        Eigen::Matrix4cd left = svd.matrixU().transpose() * left_adjust;
        Eigen::Matrix4cd right = right_adjust * svd.matrixV();

        return std::make_tuple(left, right);

        
    */
    //std::cout << "U U^="<<svd.matrixU()*svd.matrixU().transpose()<<std::endl;
    //std::cout << "T T^="<<svd.matrixV()*svd.matrixV().transpose()<<std::endl;
    return std::make_tuple(svd.matrixU().transpose(), svd.matrixV());

    // semi_correctedを分解します
}

std::tuple<Eigen::Matrix4cd, Eigen::Matrix4cd, Eigen::Matrix4cd>
bidiagonalize_unitary_with_special_orthogonals(Eigen::Matrix4cd mat) {
    Eigen::Matrix4d matA, matB;

    matA = mat.real();
    matB = mat.imag();
    auto aaa =
        bidiagonalize_real_matrix_pair_with_symmetric_products(matA, matB);
    Eigen::Matrix4cd left = std::get<0>(aaa);
    Eigen::Matrix4cd right = std::get<1>(aaa);

    
    
    Eigen::Matrix4cd diag = left * mat * right;
    std::cout << "left=" << left << std::endl;
    std::cout << "mat=" << mat << std::endl;
    std::cout << "right=" << right << std::endl;
    std::cout << "diag=" << diag << std::endl;
    if (left.determinant().real() < 0) {
        std::cerr<<"Ldet"<<std::endl;
        for (int i = 0; i < 4; i++) {
            left(0,i) *= -1;
        }
        diag(0,0)*=-1;
    }
    if (right.determinant().real() < 0) {
        std::cerr<<"Rdet"<<std::endl;
        for (int i = 0; i < 4; i++) {
            right(i, 0) *= -1;
        }
        diag(0,0)*=-1;
    }
    return std::make_tuple(left, diag, right);
}
// diag = left * KAK_MAGIC_DAG * taget * KAK_MAGIC * right
// left^T * diag * right^T = KAK_MAGIC_DAG * target * KAK_MAGIC

// left_su2 = KAK_MAGIC * left^T * KAK_MAGIC_DAG
// target = left_su2 * KAK_MAGIC*diag*KAK_MAGIC_DAG * right_su2
// diag = KAK_MAGIC_DAG *so4_XXYYZZ * KAK_MAGIC
KAK_data KAK_decomposition(QuantumGateBase* target_gate) {
    // 入力は4*4 のゲート

    Eigen::Matrix4cd left, diag,right;

    ComplexMatrix mat_moto;
    target_gate->set_matrix(mat_moto);
    Eigen::Matrix4cd mat = mat_moto;

    std::tie(left, diag, right) = bidiagonalize_unitary_with_special_orthogonals(
        KAK_MAGIC_DAG * mat * KAK_MAGIC);

    Eigen::Matrix<CPPCTYPE, 2, 2> a1, a0, b1, b0;
    tie(a0, a1) = so4_to_magic_su2s(left.transpose());
    tie(b0, b1) = so4_to_magic_su2s(right.transpose());

    CPPCTYPE w, x, y, z;
    Eigen::Matrix<CPPCTYPE, 4, 1> d_diag_angle, wxyz;
    d_diag_angle[0] = std::arg(diag(0,0));
    d_diag_angle[1] = std::arg(diag(1,1));
    d_diag_angle[2] = std::arg(diag(2,2));
    d_diag_angle[3] = std::arg(diag(3,3));

    std::cout << "d_arg=" << d_diag_angle << std::endl;

    wxyz = KAK_GAMMA * d_diag_angle;

    KAK_data ans;
    ans.global_phase = std::exp(1.0i * wxyz[0]);
    ans.interaction_coefficients[0] = wxyz[1].real()*2;
    ans.interaction_coefficients[1] = wxyz[2].real()*2;
    ans.interaction_coefficients[2] = wxyz[3].real()*2;
    std::cout << "wxyz123=" << wxyz[1] << " " << wxyz[2] << " " << wxyz[3]
              << std::endl;

    // matrixの正当性を確認したほうがいいな

    Eigen::Matrix4cd aaa = KAK_MAGIC_DAG;
    ComplexMatrix bbb;

    auto gateXX =
        gate::PauliRotation({0, 1}, {1, 1}, ans.interaction_coefficients[0]);
    gateXX->set_matrix(bbb);
    aaa *= bbb;
    delete gateXX;
    auto gateYY =
        gate::PauliRotation({0, 1}, {2, 2}, ans.interaction_coefficients[1]);
    gateYY->set_matrix(bbb);
    aaa *= bbb;
    delete gateYY;
    auto gateZZ =
        gate::PauliRotation({0, 1}, {3, 3}, ans.interaction_coefficients[2]);
    gateZZ->set_matrix(bbb);
    aaa *= bbb;
    delete gateZZ;
    aaa *= KAK_MAGIC;
    std::cout << "aaa=" << aaa << std::endl;
    std::cout << "aaa_arg=" << std::arg(aaa(0, 0)) << " " << std::arg(aaa(1, 1))
              << " " << std::arg(aaa(2, 2)) << " " << std::arg(aaa(3, 3)) << " "
              << std::endl;
              
    std::cout<<"ans.global="<<ans.global_phase<<std::endl;
    std::cout<<"LaR"<<  left.transpose() * aaa * right.transpose()*ans.global_phase<<std::endl;
    std::cout<<"mat_moto"<<KAK_MAGIC_DAG * mat * KAK_MAGIC<<std::endl;
    std::cout<<"diag="<<diag<<std::endl;
    QuantumGateMatrix* a0_gate =
        gate::DenseMatrix({target_gate->get_target_index_list()[0]}, a0);
    QuantumGateMatrix* a1_gate =
        gate::DenseMatrix({target_gate->get_target_index_list()[1]}, a1);
    QuantumGateMatrix* b0_gate =
        gate::DenseMatrix({target_gate->get_target_index_list()[0]}, b0);
    QuantumGateMatrix* b1_gate =
        gate::DenseMatrix({target_gate->get_target_index_list()[1]}, b1);
    
    ComplexMatrix debugaaa;
    auto merged=gate::merge(b0_gate,b1_gate);
    merged->set_matrix(debugaaa);
    //std::cout<<"merged="<<debugaaa<<std::endl;
    //std::cout<<"sore"<<KAK_MAGIC * right.transpose() * KAK_MAGIC_DAG<<std::endl;
    

    ans.single_qubit_operations_after[0] = a0_gate;
    ans.single_qubit_operations_after[1] = a1_gate;
    ans.single_qubit_operations_before[0] = b0_gate;
    ans.single_qubit_operations_before[1] = b1_gate;

    return ans;
}