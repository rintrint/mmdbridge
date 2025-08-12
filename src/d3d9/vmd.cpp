#ifdef WITH_VMD

#include "d3d9.h"

#include "bridge_parameter.h"
#include "UMStringUtil.h"
#include "UMPath.h"
#include "UMVector.h"
#include "UMMatrix.h"

#include <map>
#include <vector>

#include <pybind11/pybind11.h>
namespace py = pybind11;

#include <ImathMatrix.h>

#include <EncodingHelper.h>
#include <Pmd.h>
#include <Pmx.h>
#include <Vmd.h>

#include "MMDExport.h"

template <class T> std::string to_string(T value)
{
	return umbase::UMStringUtil::number_to_string(value);
}

typedef std::shared_ptr<pmd::PmdModel> PMDPtr;
typedef std::shared_ptr<pmx::PmxModel> PMXPtr;
typedef std::shared_ptr<vmd::VmdMotion> VMDPtr;

class FileDataForVMD {
public:
	FileDataForVMD() = default;
	~FileDataForVMD() = default;

	VMDPtr vmd;
	PMDPtr pmd;
	PMXPtr pmx;
	std::map<int, int> parent_index_map;
	std::map<int, std::string> bone_name_map;
	std::map<int, int> physics_bone_map;
	std::map<int, int> ik_bone_map;
	std::map<int, int> ik_frame_bone_map;
	std::map<int, int> fuyo_bone_map;
	std::map<int, int> fuyo_target_map;
	std::map<int, UMMat44d> initial_world_matrices;

	FileDataForVMD(const FileDataForVMD& data) {
		this->vmd = data.vmd;
		this->pmd = data.pmd;
		this->pmx = data.pmx;
		this->parent_index_map = data.parent_index_map;
		this->bone_name_map = data.bone_name_map;
		this->physics_bone_map = data.physics_bone_map;
		this->ik_bone_map = data.ik_bone_map;
		this->ik_frame_bone_map = data.ik_frame_bone_map;
		this->fuyo_bone_map = data.fuyo_bone_map;
		this->fuyo_target_map = data.fuyo_target_map;
		this->initial_world_matrices = data.initial_world_matrices;
	}
};

class VMDArchive {
public:

	static VMDArchive& instance() {
		static VMDArchive instance;
		return instance;
	}

	std::map<std::string, unsigned long long> file_path_map;

	std::vector<FileDataForVMD> data_list;

	std::string output_path;

	int export_mode = 0;

	void end()
	{
		data_list.clear();
		file_path_map.clear();
		output_path.clear();
	}

	~VMDArchive() = default;
private:
	VMDArchive() = default;
};

static bool start_vmd_export(
	const std::string& directory_path,
	int export_mode)
{
	VMDArchive &archive = VMDArchive::instance();
	BridgeParameter::mutable_instance().is_exporting_without_mesh = true;
	const BridgeParameter& parameter = BridgeParameter::instance();
	if (parameter.export_fps <= 0)
	{
		return false;
	}
	if (const std::string& output_path(directory_path); output_path.empty())
	{
		VMDArchive::instance().output_path = oguna::EncodingConverter::wstringTostring(parameter.base_path) + ("out/");
	}

	archive.export_mode = export_mode;
	const int pmd_num = ExpGetPmdNum();
	for (int i = 0; i < pmd_num; ++i) {
		const char* filename = ExpGetPmdFilename(i);
		if (archive.file_path_map.find(filename) != archive.file_path_map.end()) {
			continue;
		}
		if (const PMDPtr pmd = pmd::PmdModel::LoadFromFile(filename))
		{
			FileDataForVMD data;
			data.pmd = pmd;
			archive.data_list.push_back(data);
			archive.file_path_map[filename] = archive.data_list.size() - 1ULL;
		}
		else
		{
			const auto pmx = std::make_shared<pmx::PmxModel>();
			std::wstring filename_wstring;
			oguna::EncodingConverter::Cp932ToUtf16(filename, static_cast<int>(strnlen(filename, 4096)), &filename_wstring);
			std::ifstream stream(filename_wstring, std::ios_base::binary);
			if (stream.good())
			{
				pmx->Init();
				pmx->Read(&stream);
			}
			FileDataForVMD data;
			data.pmx = pmx;
			archive.data_list.push_back(data);
			archive.file_path_map[filename] = archive.data_list.size() - 1;
		}
	}

	return true;
}

static bool end_vmd_export()
{
	VMDArchive &archive = VMDArchive::instance();
	BridgeParameter::mutable_instance().is_exporting_without_mesh = true;
	BridgeParameter::instance();
	const int pmd_num = ExpGetPmdNum();

	for (int i = 0; i < pmd_num; ++i)
	{
		const char* filename = ExpGetPmdFilename(i);
		FileDataForVMD& file_data = archive.data_list.at(archive.file_path_map[filename]);
		if (file_data.vmd)
		{
			std::string dst;
			oguna::EncodingConverter::Cp932ToUtf8(filename, static_cast<int>(strnlen(filename, 4096)), &dst);
			const umstring umstr = umbase::UMStringUtil::utf8_to_utf16(dst);
			umstring filename_string = umbase::UMPath::get_file_name(umstr);
			const umstring extension = umbase::UMStringUtil::utf8_to_utf16(".vmd");
			filename_string.replace(filename_string.size() - 4, 4, extension);
			auto output_filepath = umbase::UMStringUtil::utf16_to_wstring(umbase::UMStringUtil::utf8_to_utf16(archive.output_path) + filename_string);
			file_data.vmd->SaveToFile(output_filepath);
		}
	}

	VMDArchive::instance().end();
	return true;
}

static void init_file_data(FileDataForVMD& data)
{
	if (data.pmd) {
		const std::vector<pmd::PmdBone>& bones = data.pmd->bones;
		const std::vector<pmd::PmdRigidBody>& rigids = data.pmd->rigid_bodies;
		std::map<int, int> bone_to_rigid_map;
		for (int i = 0, isize = static_cast<int>(bones.size()); i < isize; ++i)
		{
			const pmd::PmdBone& bone = bones[i];
			const int parent_bone = bone.parent_bone_index;
			data.parent_index_map[i] = parent_bone;
			data.bone_name_map[i] = bone.name;
			if (bone.bone_type == pmd::BoneType::IkEffectable)
			{
				data.ik_bone_map[i] = 1;
			}
			if (bone.bone_type == pmd::BoneType::IkEffector)
			{
				data.ik_frame_bone_map[i] = 1;
			}
		}

		for (int i = 0, isize = static_cast<int>(rigids.size()); i < isize; ++i)
		{
			const pmd::PmdRigidBody& rigid = rigids[i];
			const uint16_t bone_index = rigid.related_bone_index;
			bone_to_rigid_map[bone_index] = i;
			if (rigid.rigid_type != pmd::RigidBodyType::BoneConnected)
			{
				if (data.bone_name_map.find(bone_index) != data.bone_name_map.end())
				{
					if (rigid.rigid_type == pmd::RigidBodyType::ConnectedPhysics)
					{
						data.physics_bone_map[bone_index] = 2;
					}
					else
					{
						data.physics_bone_map[bone_index] = 1;
					}
				}
			}
		}
		// expect for rigid_type == BoneConnected
		{
			std::vector<int> parent_physics_bone_list;
			for (const auto& rigid : rigids)
			{
				const int target_bone = rigid.related_bone_index;
				const int parent_bone = data.parent_index_map[target_bone];
				if (data.physics_bone_map.find(target_bone) != data.physics_bone_map.end())
				{
					if (bone_to_rigid_map.find(parent_bone) != bone_to_rigid_map.end())
					{
						parent_physics_bone_list.push_back(parent_bone);
					}
				}
			}
			for (int parent_bone : parent_physics_bone_list)
			{
				const pmd::PmdRigidBody& parent_rigid = rigids[bone_to_rigid_map[parent_bone]];
				if (parent_rigid.rigid_type == pmd::RigidBodyType::BoneConnected) {
					data.physics_bone_map[parent_bone] = 0;
				}
			}
		}
	}
	else if (data.pmx)
	{
		const int bone_count = static_cast<int>(data.pmx->bones.size());
		for (int i = 0; i < bone_count; ++i)
		{
			const pmx::PmxBone& bone = data.pmx->bones[i];
			const int parent_bone = bone.parent_index;
			data.parent_index_map[i] = parent_bone;
			oguna::EncodingConverter::Utf16ToCp932(bone.bone_name.c_str(), static_cast<int>(bone.bone_name.length()), &data.bone_name_map[i]);
			for (int k = 0; k < bone.ik_link_count; ++k)
			{
				const pmx::PmxIkLink& link = bone.ik_links[k];
				data.ik_bone_map[link.link_target] = 1;
				data.ik_frame_bone_map[i] = 1;
			}
			if ((bone.bone_flag & 0x0100) || (bone.bone_flag & 0x0200)) {
				data.fuyo_target_map[bone.grant_parent_index] = 1;
				data.fuyo_bone_map[i] = 1;
			}
		}

		const int rigid_count = static_cast<int>(data.pmx->rigid_bodies.size());
		std::map<int, int> bone_to_rigid_map;
		for (int i = 0; i < rigid_count; ++i)
		{
			const pmx::PmxRigidBody& rigid = data.pmx->rigid_bodies[i];
			bone_to_rigid_map[rigid.target_bone] = i;
			if (rigid.physics_calc_type != 0)
			{
				const auto bone_index = rigid.target_bone;
				if (data.bone_name_map.find(bone_index) != data.bone_name_map.end())
				{
					if (rigid.physics_calc_type == 2) {
						data.physics_bone_map[bone_index] = 2;
					}
					else
					{
						data.physics_bone_map[bone_index] = 1;
					}
				}
			}
		}
		// expect for physics_calc_type == 0
		{
			std::vector<int> parent_physics_bone_list;
			for (int i = 0; i < rigid_count; ++i)
			{
				const pmx::PmxRigidBody& rigid = data.pmx->rigid_bodies[i];
				const int target_bone = rigid.target_bone;
				const int parent_bone = data.parent_index_map[target_bone];
				if (data.physics_bone_map.find(target_bone) != data.physics_bone_map.end())
				{
					if (bone_to_rigid_map.find(parent_bone) != bone_to_rigid_map.end())
					{
						parent_physics_bone_list.push_back(parent_bone);
					}
				}
			}
			for (int parent_bone : parent_physics_bone_list)
			{
				const pmx::PmxRigidBody& parent_rigid = data.pmx->rigid_bodies[bone_to_rigid_map[parent_bone]];
				if (parent_rigid.physics_calc_type == 0) {
					data.physics_bone_map[parent_bone] = 0;
				}
			}
		}
	}
}

static UMMat44d to_ummat(const D3DMATRIX& mat)
{
	UMMat44d ummat;
	for (int n = 0; n < 4; ++n)
	{
		for (int m = 0; m < 4; ++m)
		{
			ummat[n][m] = static_cast<double>(mat.m[n][m]);
		}
	}
	return ummat;
}

// from imath
static UMVec4d extractQuat(const UMMat44d &mat)
{
	double s;
	UMVec4d   quat;
	constexpr int nxt[3] = { 1, 2, 0 };

	// check the diagonal
	if (const double tr = mat[0][0] + mat[1][1] + mat[2][2]; tr > 0.0) {
		s = sqrt(tr + 1.0);
		quat.w = s / 2.0;
		s = 0.5 / s;

		quat.x = (mat[1][2] - mat[2][1]) * s;
		quat.y = (mat[2][0] - mat[0][2]) * s;
		quat.z = (mat[0][1] - mat[1][0]) * s;
	}
	else {
		double q[4];
		// diagonal is negative
		int i = 0;
		if (mat[1][1] > mat[0][0])
			i = 1;
		if (mat[2][2] > mat[i][i])
			i = 2;

		const int j = nxt[i];
		const int k = nxt[j];
		s = sqrt((mat[i][i] - (mat[j][j] + mat[k][k])) + 1.0);

		q[i] = s * 0.5;
		if (s != 0.0)
			s = 0.5 / s;

		q[3] = (mat[j][k] - mat[k][j]) * s;
		q[j] = (mat[i][j] + mat[j][i]) * s;
		q[k] = (mat[i][k] + mat[k][i]) * s;

		quat.x = q[0];
		quat.y = q[1];
		quat.z = q[2];
		quat.w = q[3];
	}

	return quat;
}

static bool execute_vmd_export(const int currentframe)
{
	VMDArchive &archive = VMDArchive::instance();
	BridgeParameter::mutable_instance().is_exporting_without_mesh = true;

	const BridgeParameter& parameter = BridgeParameter::instance();

	const int pmd_num = ExpGetPmdNum();

	// 初始化日誌文件
	static std::ofstream log_file;
	if (currentframe == parameter.start_frame)
	{
		for (int i = 0; i < pmd_num; ++i)
		{
			const char* filename = ExpGetPmdFilename(i);
			FileDataForVMD& file_data = archive.data_list.at(archive.file_path_map[filename]);
			init_file_data(file_data);

			file_data.vmd = std::make_unique<vmd::VmdMotion>();
			if (file_data.pmd)
			{
				file_data.vmd->model_name = file_data.pmd->header.name;
			}
			else if (file_data.pmx)
			{
				oguna::EncodingConverter::Utf16ToCp932(file_data.pmx->model_name.c_str(), static_cast<int>(file_data.pmx->model_name.length()), &file_data.vmd->model_name);
			}

			const int bone_num = ExpGetPmdBoneNum(i);
			for (int k = 0; k < bone_num; ++k)
			{
                // 取得並儲存第 k 個骨骼的初始世界矩陣
				file_data.initial_world_matrices[k] = to_ummat(ExpGetPmdBoneWorldMat(i, k));
			}
		}
		// 開啟原始數據日誌文件
		std::string log_path = archive.output_path + "raw_data.txt";
		log_file.open(log_path, std::ios::out);
	}

	for (int i = 0; i < pmd_num; ++i)
	{
		const char* filename = ExpGetPmdFilename(i);
		FileDataForVMD& file_data = archive.data_list.at(archive.file_path_map[filename]);
		const int bone_num = ExpGetPmdBoneNum(i);
		for (int k = 0; k < bone_num; ++k)
		{
			UMVec3f initial_trans;
			const char* bone_name = ExpGetPmdBoneName(i, k);
			
			
            // =================================================================
            // START: 建議的除錯程式碼
            // =================================================================
			if (log_file.is_open()) {
				log_file << "====== DEBUGGING '" << bone_name << "' (BONE " << k << ") at FRAME " << currentframe << " ======\n";
				
				bool is_in_map = file_data.bone_name_map.count(k) > 0;
				log_file << "DEBUG: Is in bone_name_map? " << (is_in_map ? "Yes" : "No") << "\n";
				if (is_in_map) {
						log_file << "DEBUG: Name in map: '" << file_data.bone_name_map[k] << "', Name from Exp: '" << bone_name << "'\n";
						log_file << "DEBUG: Map name matches Exp name? " << (file_data.bone_name_map[k] == bone_name ? "Yes" : "No") << "\n";
				}

				log_file << "DEBUG: Is IK effector (ik_frame_bone_map)? " << (file_data.ik_frame_bone_map.count(k) > 0 ? "Yes, will be skipped." : "No") << "\n";
				log_file << "DEBUG: Is physics bone (physics_bone_map)? " << (file_data.physics_bone_map.count(k) > 0 ? ("Yes, type " + to_string(file_data.physics_bone_map[k])) : "No") << "\n";
				log_file << "DEBUG: Is IK bone (ik_bone_map)? " << (file_data.ik_bone_map.count(k) > 0 ? "Yes" : "No") << "\n";
				log_file << "DEBUG: Is Fuyo bone (fuyo_bone_map)? " << (file_data.fuyo_bone_map.count(k) > 0 ? "Yes" : "No") << "\n";
				log_file << "DEBUG: Current export_mode: " << archive.export_mode << "\n";
			}
            // =================================================================
            // END: 建議的除錯程式碼
            // =================================================================

			
			if (file_data.bone_name_map.find(k) == file_data.bone_name_map.end()) {
				continue;
			}

			if (file_data.bone_name_map[k] != bone_name) {
				continue;
			}

			// export mode
			const bool is_ik_effector_bone = file_data.ik_frame_bone_map.count(k) > 0;
			const bool is_affected_by_ik = file_data.ik_bone_map.count(k) > 0;
			const bool is_fuyo_effector_bone = file_data.fuyo_target_map.find(k) != file_data.fuyo_target_map.end();
			const bool is_affected_by_fuyo = file_data.fuyo_bone_map.count(k) > 0;
			const bool is_physics_bone = file_data.physics_bone_map.count(k) > 0;
			bool is_simulated_physics_bone = false;
			bool is_non_simulated_physics_bone = false;
			if (is_physics_bone) {
				if (file_data.physics_bone_map.at(k) == 0) {
					is_non_simulated_physics_bone = true;
				} else {
					is_simulated_physics_bone = true;
				}
			}
			// Since IK is baked to FK, skip exporting IK bone motion keyframes
			if (is_ik_effector_bone) {
				continue;
			}
			if (is_fuyo_effector_bone) {
				continue;
			}
			if (archive.export_mode == 0) {
				// physics + ik + fuyo
				if (!is_simulated_physics_bone && !is_affected_by_ik && !is_affected_by_fuyo) {
					continue;
				}
			}
			else if (archive.export_mode == 1) {
				// physics only
				if (!is_simulated_physics_bone) {
					continue;
				}
			}
			else {
				// all
			}

			// get initial world position
			if (file_data.pmd)
			{
				const pmd::PmdBone& bone = file_data.pmd->bones[k];
				if (bone.bone_type == pmd::BoneType::Invisible)
				{
					continue;
				}
				initial_trans[0] = bone.bone_head_pos[0];
				initial_trans[1] = bone.bone_head_pos[1];
				initial_trans[2] = bone.bone_head_pos[2];
			}
			else if (file_data.pmx)
			{
				const pmx::PmxBone& bone = file_data.pmx->bones[k];
				initial_trans[0] = bone.position[0];
				initial_trans[1] = bone.position[1];
				initial_trans[2] = bone.position[2];
			}

			// 獲取原始數據
			UMMat44d world = to_ummat(ExpGetPmdBoneWorldMat(i, k));
			UMMat44d parent_world;
			parent_world.identity(); // 預設為單位矩陣

			UMMat44d local = world;
			int parent_index = file_data.parent_index_map[k];
			if (parent_index != 0xFFFF && file_data.parent_index_map.find(parent_index) != file_data.parent_index_map.end()) {
				parent_world = to_ummat(ExpGetPmdBoneWorldMat(i, parent_index));
				local = world * parent_world.inverted();
			}

			// 寫入原始數據日誌 (Python 友好格式)
			if (log_file.is_open()) {
				log_file << std::fixed << std::setprecision(3);

				log_file << "FRAME:" << currentframe << ",BONE:" << k << ",NAME:" << bone_name << "\n";

				// 初始位置
				log_file << "INIT_POS:" << initial_trans[0] << "," << initial_trans[1] << "," << initial_trans[2] << "\n";

				// 初始旋轉
				UMMat44d initial_world_mat = file_data.initial_world_matrices[k];
				UMVec4d initial_rot_quat = extractQuat(initial_world_mat);
				log_file << "INIT_ROT:" << initial_rot_quat.x << "," << initial_rot_quat.y << "," << initial_rot_quat.z << "," << initial_rot_quat.w << "\n";

				// 世界矩陣 (4x4)
				log_file << "WORLD_MAT:";
				for (int row = 0; row < 4; ++row) {
					for (int col = 0; col < 4; ++col) {
						log_file << world[row][col];
						if (row < 3 || col < 3) log_file << ",";
					}
				}
				log_file << "\n";

				// 父骨骼世界矩陣
				log_file << "PARENT_MAT:";
				for (int row = 0; row < 4; ++row) {
					for (int col = 0; col < 4; ++col) {
						log_file << parent_world[row][col];
						if (row < 3 || col < 3) log_file << ",";
					}
				}
				log_file << "\n";
                
                log_file << "LOCAL_TRANS:" << local[3][0] << "," << local[3][1] << "," << local[3][2] << "\n";
			}

			vmd::VmdBoneFrame bone_frame;
			bone_frame.frame = currentframe;
			bone_frame.name = ExpGetPmdBoneName(i, k);

            // Calculate VMD position
            // Step 1: Calculate basic displacement offset (current local position - own initial position)
            bone_frame.position[0] = static_cast<float>(local[3][0]) - initial_trans[0];
            bone_frame.position[1] = static_cast<float>(local[3][1]) - initial_trans[1];
            bone_frame.position[2] = static_cast<float>(local[3][2]) - initial_trans[2];
			if (log_file.is_open()) {
				log_file << "VMD_POS_TEMP:" << bone_frame.position[0] << "," << bone_frame.position[1] << "," << bone_frame.position[2] << "\n";
			}
            // Step 2: Add parent bone's initial position
			if (parent_index != 0xFFFF && file_data.parent_index_map.find(parent_index) != file_data.parent_index_map.end()) {
                UMVec3f parent_initial_trans;
                if (file_data.pmd)
                {
                    const pmd::PmdBone& parent_bone = file_data.pmd->bones[parent_index];
                    parent_initial_trans[0] = parent_bone.bone_head_pos[0];
                    parent_initial_trans[1] = parent_bone.bone_head_pos[1];
                    parent_initial_trans[2] = parent_bone.bone_head_pos[2];
                }
                else if (file_data.pmx)
                {
                    const pmx::PmxBone& parent_bone = file_data.pmx->bones[parent_index];
                    parent_initial_trans[0] = parent_bone.position[0];
                    parent_initial_trans[1] = parent_bone.position[1];
                    parent_initial_trans[2] = parent_bone.position[2];
                }
                bone_frame.position[0] += parent_initial_trans[0];
                bone_frame.position[1] += parent_initial_trans[1];
                bone_frame.position[2] += parent_initial_trans[2];
            }

			local[3][0] = local[3][1] = local[3][2] = 0.0;
			UMVec4d quat = extractQuat(local);
			bone_frame.orientation[0] = static_cast<float>(quat[0]);
			bone_frame.orientation[1] = static_cast<float>(quat[1]);
			bone_frame.orientation[2] = static_cast<float>(quat[2]);
			bone_frame.orientation[3] = static_cast<float>(quat[3]);

			// 記錄計算後的 VMD 數據
			if (log_file.is_open()) {
				log_file << "VMD_POS:" << bone_frame.position[0] << "," << bone_frame.position[1] << "," << bone_frame.position[2] << "\n";
				log_file << "VMD_ROT:" << bone_frame.orientation[0] << "," << bone_frame.orientation[1] << "," << bone_frame.orientation[2] << "," << bone_frame.orientation[3] << "\n";
			}

			for (auto& n : bone_frame.interpolation)
			{
				for (int m = 0; m < 4; ++m) {
					n[0][m] = 20;
					n[1][m] = 20;
				}
				for (int m = 0; m < 4; ++m) {
					n[2][m] = 107;
					n[3][m] = 107;
				}
			}

			// The following code is disabled.
			// This constraint logic is unnecessary for MMD:
			// 1. Bones can still translate/rotate regardless of restrictions, especially certain physic bones such as chest bones.
			// 2. Baked animations already capture MMD's final movement.
			/////////////////////////////////////////////////////////////////
			// // constraints
			// if (file_data.pmd)
			// {
			// 	// The bone is not translatable.
			// 	if (const pmd::PmdBone& bone = file_data.pmd->bones[k]; bone.bone_type == pmd::BoneType::Rotation)
			// 	{
			// 		bone_frame.position[0] = 0.0f;
			// 		bone_frame.position[1] = 0.0f;
			// 		bone_frame.position[2] = 0.0f;
			// 	}
			// }
			// else if (file_data.pmx)
			// {
			// 	const pmx::PmxBone& bone = file_data.pmx->bones[k];
			// 	// The bone is not translatable.
			// 	if (!(bone.bone_flag & 0x0004))
			// 	{
			// 		bone_frame.position[0] = 0.0f;
			// 		bone_frame.position[1] = 0.0f;
			// 		bone_frame.position[2] = 0.0f;
			// 	}
			// 	// The bone is not rotatable.
			// 	if (!(bone.bone_flag & 0x0002))
			// 	{
			// 		bone_frame.orientation[0] = 0.0f;
			// 		bone_frame.orientation[1] = 0.0f;
			// 		bone_frame.orientation[2] = 0.0f;
			// 		bone_frame.orientation[3] = 1.0f;
			// 	}
			// 	if (file_data.physics_bone_map.find(k) != file_data.physics_bone_map.end()) {
			// 		if (file_data.physics_bone_map[k] == 2)
			// 		{
			// 			bone_frame.position[0] = 0.0f;
			// 			bone_frame.position[1] = 0.0f;
			// 			bone_frame.position[2] = 0.0f;
			// 		}
			// 	}
			// 	// expect for rigid_type == BoneConnected
			// 	const int parent_bone = file_data.parent_index_map[k];
			// 	if (file_data.physics_bone_map.find(parent_bone) != file_data.physics_bone_map.end()) {
			// 		if (file_data.physics_bone_map[parent_bone] == 0) {
			// 			bone_frame.position[0] = 0.0f;
			// 			bone_frame.position[1] = 0.0f;
			// 			bone_frame.position[2] = 0.0f;
			// 		}
			// 	}
			// }

			// 記錄約束後的最終數據
			if (log_file.is_open()) {
				log_file << "FINAL_POS:" << bone_frame.position[0] << "," << bone_frame.position[1] << "," << bone_frame.position[2] << "\n";
				log_file << "FINAL_ROT:" << bone_frame.orientation[0] << "," << bone_frame.orientation[1] << "," << bone_frame.orientation[2] << "," << bone_frame.orientation[3] << "\n";
				log_file << "\n";
			}

			file_data.vmd->bone_frames.push_back(bone_frame);
		}


		if (currentframe == parameter.start_frame)
		{
			vmd::VmdIkFrame ik_frame;
			ik_frame.frame = currentframe;
			ik_frame.display = true;
			for (auto it = file_data.ik_frame_bone_map.begin(); it != file_data.ik_frame_bone_map.end(); ++it)
			{
				if (file_data.bone_name_map.find(it->first) != file_data.bone_name_map.end())
				{
					vmd::VmdIkEnable ik_enable;
					ik_enable.ik_name = file_data.bone_name_map[it->first];
					ik_enable.enable = false;
					ik_frame.ik_enable.push_back(ik_enable);
				}
			}
			file_data.vmd->ik_frames.push_back(ik_frame);
		}
	}

	// 在最後一幀時關閉日誌文件
	if (currentframe == parameter.end_frame && log_file.is_open()) {
		log_file.close();
	}

	return true;
}

// ---------------------------------------------------------------------------
PYBIND11_PLUGIN(mmdbridge_vmd) {
	py::module m("mmdbridge_vmd");
	m.def("start_vmd_export", start_vmd_export);
	m.def("end_vmd_export", end_vmd_export);
	m.def("execute_vmd_export", execute_vmd_export);
	return m.ptr();
}

#endif //WITH_VMD

// ---------------------------------------------------------------------------
#ifdef WITH_VMD
void InitVMD()
	{
		PyImport_AppendInittab("mmdbridge_vmd", PyInit_mmdbridge_vmd);
	}
	void DisposeVMD()
	{
		VMDArchive::instance().end();
	}
#else
	void InitVMD(){}
	void DisposeVMD() {}
#endif //WITH_VMD
