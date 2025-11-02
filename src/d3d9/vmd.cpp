#include "vmd.h"

#ifdef WITH_VMD

#include "d3d9.h"

#include <shlwapi.h>

#include "bridge_parameter.h"
#include "UMStringUtil.h"
#include "UMPath.h"

#include <map>
#include <vector>

#include <pybind11/pybind11.h>
namespace py = pybind11;

#include <ImathMatrix.h>
#include <ImathMatrixAlgo.h>
#include <ImathQuat.h>
#include <ImathVec.h>

#include <EncodingHelper.h>
#include <Pmd.h>
#include <Pmx.h>
#include <Vmd.h>

template <class T>
std::string to_string(T value)
{
	return umbase::UMStringUtil::number_to_string(value);
}

typedef std::shared_ptr<pmd::PmdModel> PMDPtr;
typedef std::shared_ptr<pmx::PmxModel> PMXPtr;
typedef std::shared_ptr<vmd::VmdMotion> VMDPtr;

static void ShowInvalidBoneNameError()
{
	MessageBoxW(NULL,
				L"Invalid bone name detected.\n\n"
				L"Bone names must comply with CP932 encoding and cannot exceed 15 bytes. "
				L"Please use tools such as Blender MMD Tools to fix the bone names.\n\n"
				L"For more information, please refer to the tutorials:\n"
				L"https://github.com/rintrint/mmdbridge/blob/master/docs/how_to_use.md\n"
				L"https://www.bilibili.com/opus/1102730546871533640\n\n"
				L"(Press Ctrl+C to copy this message)",
				L"Error",
				MB_OK | MB_ICONERROR);
}

class FileDataForVMD
{
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
	// morph (face)
	std::map<int, std::string> morph_name_map;

	FileDataForVMD(const FileDataForVMD& data)
	{
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
		// morph (face)
		this->morph_name_map = data.morph_name_map;
	}
};

class VMDArchive
{
public:
	static VMDArchive& instance()
	{
		static VMDArchive instance;
		return instance;
	}

	std::vector<FileDataForVMD> data_list;
	std::wstring output_path;
	bool has_bone_name_error = false;

	// export settings
	int export_fk_bone_animation_mode = 1;
	bool export_ik_bone_animation = false;
	bool add_turn_off_ik_keyframe = true;
	bool export_morph_animation = true;

	void end()
	{
		data_list.clear();
		output_path.clear();
		has_bone_name_error = false;
	}

	~VMDArchive() = default;

private:
	VMDArchive() = default;
};

static bool start_vmd_export(
	const int export_fk_bone_animation_mode,
	const bool export_ik_bone_animation,
	const bool add_turn_off_ik_keyframe,
	const bool export_morph_animation)
{
	// Clear previous export to ensure a clean state
	VMDArchive::instance().end();

	VMDArchive& archive = VMDArchive::instance();
	const BridgeParameter& parameter = BridgeParameter::instance();
	if (parameter.export_fps <= 0)
	{
		return false;
	}

	std::wstring output_path = parameter.base_path + L"out/";

	// Make sure the output folder exists.
	if (!CreateDirectoryW(output_path.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		std::wstring error_message = L"Cannot create output folder: " + output_path;
		::MessageBoxW(NULL, error_message.c_str(), L"Error", MB_OK | MB_ICONERROR);
	}

	VMDArchive::instance().output_path = output_path;

	archive.export_fk_bone_animation_mode = export_fk_bone_animation_mode;
	archive.export_ik_bone_animation = export_ik_bone_animation;
	archive.add_turn_off_ik_keyframe = add_turn_off_ik_keyframe;
	archive.export_morph_animation = export_morph_animation;

	const int pmd_num = ExpGetPmdNum();
	for (int i = 0; i < pmd_num; ++i)
	{
		const char* filepath = ExpGetPmdFilenameUtf8(i);
		std::wstring filepath_wstring = umbase::UMStringUtil::utf16_to_wstring(umbase::UMStringUtil::utf8_to_utf16(filepath));
		std::wstring file_ext_wstring = PathFindExtensionW(filepath_wstring.c_str());
		std::transform(file_ext_wstring.begin(), file_ext_wstring.end(), file_ext_wstring.begin(), ::towlower);

		if (file_ext_wstring == L".pmd")
		{
			PMDPtr pmd;
			if ((pmd = pmd::PmdModel::LoadFromFile(filepath)))
			{
			}
			else
			{
				std::wstring error_message = L"Failed to load pmd file: " + filepath_wstring;
				::MessageBoxW(NULL, error_message.c_str(), L"Error", MB_OK | MB_ICONERROR);
			}
			FileDataForVMD data;
			data.pmd = pmd;
			archive.data_list.push_back(data);
		}
		else if (file_ext_wstring == L".pmx")
		{
			const auto pmx = std::make_shared<pmx::PmxModel>();
			std::ifstream stream(filepath_wstring, std::ios_base::binary);
			if (stream.good())
			{
				pmx->Init();
				pmx->Read(&stream);
			}
			else
			{
				std::wstring error_message = L"Failed to open pmx file: " + filepath_wstring;
				::MessageBoxW(NULL, error_message.c_str(), L"Error", MB_OK | MB_ICONERROR);
			}
			FileDataForVMD data;
			data.pmx = pmx;
			archive.data_list.push_back(data);
		}
		else
		{
			std::wstring error_message;
			if (filepath_wstring.empty())
			{
				error_message = L"Unable to get pmd/pmx filepath.";
			}
			else
			{
				error_message = L"This is not a pmd/pmx file: " + filepath_wstring;
			}
			::MessageBoxW(NULL, error_message.c_str(), L"Error", MB_OK | MB_ICONERROR);
		}
	}

	return true;
}

/**
 * @brief 通用的關鍵幀後處理與過濾輔助函數
 * @tparam T 幀的類型
 * @tparam GetNameFunc 獲取名稱的函式類型
 * @tparam AreEqualFunc 比較相等的函式類型
 * @tparam IsZeroFunc 判斷是否為零值的函式類型
 */
template <typename T, typename GetNameFunc, typename AreEqualFunc, typename IsZeroFunc>
std::vector<T> PostProcessKeyframes(
	const std::vector<T>& all_frames,
	GetNameFunc get_name_func,
	AreEqualFunc are_equal_func,
	IsZeroFunc is_zero_func)
{
	if (all_frames.empty())
	{
		return {};
	}

	// 將所有幀按其名稱分組
	std::map<std::string, std::vector<T>> grouped_frames;
	for (const auto& frame : all_frames)
	{
		grouped_frames[get_name_func(frame)].push_back(frame);
	}

	std::vector<T> final_frames;
	final_frames.reserve(all_frames.size());

	// 對每一組 (每一個骨骼或表情) 獨立進行過濾
	for (auto const& [name, frames] : grouped_frames)
	{
		if (frames.empty())
		{
			continue;
		}

		// ------------------------- 過濾規則實施開始 -------------------------
		// 規則 1: 先清除相同的中間幀
		std::vector<T> stage1_frames;
		if (frames.size() > 2)
		{
			stage1_frames.push_back(frames.front()); // 總是保留第一幀
			for (int i = 1; i < frames.size() - 1; ++i)
			{
				// 如果一個幀和它前後的幀都相等，它就是可移除的中間幀
				if (!(are_equal_func(frames[i - 1], frames[i]) && are_equal_func(frames[i], frames[i + 1])))
				{
					stage1_frames.push_back(frames[i]);
				}
			}
			stage1_frames.push_back(frames.back()); // 總是保留最後一幀
		}
		else
		{
			stage1_frames = frames; // 幀數小於等於2，沒有中間幀
		}

		// 規則 2: 如果只剩下頭尾且頭尾一樣, 留下頭
		std::vector<T> stage2_frames;
		if (stage1_frames.size() == 2 && are_equal_func(stage1_frames[0], stage1_frames[1]))
		{
			stage2_frames.push_back(stage1_frames[0]);
		}
		else
		{
			stage2_frames = stage1_frames;
		}

		// 規則 3: 如果只剩下頭 且頭為0.0 直接刪除
		if (stage2_frames.size() == 1 && is_zero_func(stage2_frames[0]))
		{
			// 不做任何事，即刪除這個表情的所有關鍵幀
		}
		else
		{
			// 將最終結果合併到 final_frames
			final_frames.insert(final_frames.end(), stage2_frames.begin(), stage2_frames.end());
		}
		// ------------------------- 過濾規則實施結束 -------------------------
	}

	// 最終結果按幀號排序
	std::sort(final_frames.begin(), final_frames.end(),
			  [](const T& a, const T& b) {
				  return a.frame < b.frame;
			  });

	return final_frames;
}

static bool end_vmd_export()
{
	VMDArchive& archive = VMDArchive::instance();
	if (archive.has_bone_name_error)
	{
		archive.end();
		ShowInvalidBoneNameError();
		return false;
	}

	const int pmd_num = ExpGetPmdNum();
	std::map<std::wstring, int> output_name_counts;
	for (int i = 0; i < pmd_num; ++i)
	{
		FileDataForVMD& file_data = archive.data_list.at(i);
		if (!file_data.vmd)
		{
			continue;
		}

		// PostProcess
		{
			// morph (face)
			const float face_threshold = 0.0f;
			auto get_face_name = [](const vmd::VmdFaceFrame& f) { return f.face_name; };
			auto are_faces_equal = [&](const vmd::VmdFaceFrame& a, const vmd::VmdFaceFrame& b) {
				return std::abs(a.weight - b.weight) <= face_threshold;
			};
			auto is_face_zero = [&](const vmd::VmdFaceFrame& f) {
				return std::abs(f.weight) <= face_threshold;
			};
			file_data.vmd->face_frames = PostProcessKeyframes(file_data.vmd->face_frames, get_face_name, are_faces_equal, is_face_zero);

			// bone
			const float bone_threshold = 0.0f;
			auto get_bone_name = [](const vmd::VmdBoneFrame& f) { return f.name; };
			auto are_bones_equal = [&](const vmd::VmdBoneFrame& a, const vmd::VmdBoneFrame& b) {
				for (int j = 0; j < 3; ++j)
				{
					if (std::abs(a.position[j] - b.position[j]) > bone_threshold)
						return false;
				}
				for (int j = 0; j < 4; ++j)
				{
					if (std::abs(a.orientation[j] - b.orientation[j]) > bone_threshold)
						return false;
				}
				return true;
			};
			auto is_bone_zero = [&](const vmd::VmdBoneFrame& f) {
				bool position_is_zero = std::abs(f.position[0]) <= bone_threshold &&
										std::abs(f.position[1]) <= bone_threshold &&
										std::abs(f.position[2]) <= bone_threshold;
				bool orientation_is_identity = std::abs(f.orientation[0]) <= bone_threshold &&
											   std::abs(f.orientation[1]) <= bone_threshold &&
											   std::abs(f.orientation[2]) <= bone_threshold &&
											   std::abs(f.orientation[3] - 1.0f) <= bone_threshold;
				return position_is_zero && orientation_is_identity;
			};
			file_data.vmd->bone_frames = PostProcessKeyframes(file_data.vmd->bone_frames, get_bone_name, are_bones_equal, is_bone_zero);
		}

		const char* filepath = ExpGetPmdFilenameUtf8(i);
		std::wstring filepath_wstring = umbase::UMStringUtil::utf16_to_wstring(umbase::UMStringUtil::utf8_to_utf16(filepath));
		std::wstring filename_wstring = PathFindFileNameW(filepath_wstring.c_str());
		if (filename_wstring.empty())
		{
			std::wstring error_message = L"Unable to get pmd/pmx filepath.";
			::MessageBoxW(NULL, error_message.c_str(), L"Error", MB_OK | MB_ICONERROR);
			continue;
		}

		WCHAR filename_buffer[MAX_PATH];
		wcscpy_s(filename_buffer, MAX_PATH, filename_wstring.c_str());
		PathRenameExtensionW(filename_buffer, L".vmd");
		std::wstring base_output_filename(filename_buffer);

		// Resolves filename collisions that occur when the same model is imported multiple times.
		int count = output_name_counts[base_output_filename]++;
		std::wstring final_output_filename = base_output_filename;
		if (count > 0)
		{
			WCHAR name_buffer[MAX_PATH];
			wcscpy_s(name_buffer, MAX_PATH, base_output_filename.c_str());
			PathRemoveExtensionW(name_buffer);
			std::wstring name_without_ext(name_buffer);
			final_output_filename = name_without_ext + L" (" + std::to_wstring(count + 1) + L").vmd";
		}

		WCHAR pathBuffer[MAX_PATH];
		PathCombineW(pathBuffer, archive.output_path.c_str(), final_output_filename.c_str());
		std::wstring output_filepath = pathBuffer;
		file_data.vmd->SaveToFile(output_filepath);
	}

	VMDArchive::instance().end();
	return true;
}

static void init_file_data(FileDataForVMD& data)
{
	if (data.pmd)
	{
		const std::vector<pmd::PmdBone>& bones = data.pmd->bones;
		const std::vector<pmd::PmdRigidBody>& rigids = data.pmd->rigid_bodies;
		std::map<int, int> bone_to_rigid_map;
		for (size_t i = 0, isize = bones.size(); i < isize; ++i)
		{
			const pmd::PmdBone& bone = bones[i];
			const uint16_t parent_bone = bone.parent_bone_index;
			data.parent_index_map[i] = (parent_bone == 0xFFFF) ? -1 : parent_bone;
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
		// morph (face)
		const std::vector<pmd::PmdFace>& faces = data.pmd->faces;
		for (size_t i = 0, isize = faces.size(); i < isize; ++i)
		{
			const pmd::PmdFace& face = faces[i];
			data.morph_name_map[i] = face.name;
		}

		for (size_t i = 0, isize = rigids.size(); i < isize; ++i)
		{
			const pmd::PmdRigidBody& rigid = rigids[i];
			const uint16_t related_bone = rigid.related_bone_index;
			const int target_bone = (related_bone == 0xFFFF) ? -1 : related_bone;
			if (target_bone < 0)
			{
				continue;
			}
			bone_to_rigid_map[target_bone] = i;
			if (rigid.rigid_type != pmd::RigidBodyType::BoneConnected)
			{
				if (data.bone_name_map.find(target_bone) != data.bone_name_map.end())
				{
					if (rigid.rigid_type == pmd::RigidBodyType::ConnectedPhysics)
					{
						data.physics_bone_map[target_bone] = 2;
					}
					else
					{
						data.physics_bone_map[target_bone] = 1;
					}
				}
			}
		}
		// expect for rigid_type == BoneConnected
		{
			std::vector<int> parent_physics_bone_list;
			for (const auto& rigid : rigids)
			{
				const uint16_t related_bone = rigid.related_bone_index;
				const int target_bone = (related_bone == 0xFFFF) ? -1 : related_bone;
				if (target_bone < 0) // Avoid dangerous map[-1] access, which auto-creates a buggy {-1, 0} mapping.
				{
					continue;
				}
				const int parent_bone = data.parent_index_map[target_bone];
				if (parent_bone < 0)
				{
					continue;
				}
				if (data.physics_bone_map.find(target_bone) != data.physics_bone_map.end() &&
					bone_to_rigid_map.find(parent_bone) != bone_to_rigid_map.end())
				{
					parent_physics_bone_list.push_back(parent_bone);
				}
			}
			for (int parent_bone : parent_physics_bone_list)
			{
				const pmd::PmdRigidBody& parent_rigid = rigids[bone_to_rigid_map[parent_bone]];
				if (parent_rigid.rigid_type == pmd::RigidBodyType::BoneConnected)
				{
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
				if (link.link_target < 0)
				{
					continue;
				}
				data.ik_bone_map[link.link_target] = 1;
				data.ik_frame_bone_map[i] = 1;
			}
			if ((bone.bone_flag & 0x0100) || (bone.bone_flag & 0x0200))
			{
				if (bone.grant_parent_index < 0)
				{
					continue;
				}
				data.fuyo_target_map[bone.grant_parent_index] = 1;
				data.fuyo_bone_map[i] = 1;
			}
		}

		const int rigid_count = static_cast<int>(data.pmx->rigid_bodies.size());
		std::map<int, int> bone_to_rigid_map;
		for (int i = 0; i < rigid_count; ++i)
		{
			const pmx::PmxRigidBody& rigid = data.pmx->rigid_bodies[i];
			const int target_bone = rigid.target_bone;
			if (target_bone < 0)
			{
				continue;
			}
			bone_to_rigid_map[target_bone] = i;
			if (rigid.physics_calc_type != 0)
			{
				if (data.bone_name_map.find(target_bone) != data.bone_name_map.end())
				{
					if (rigid.physics_calc_type == 2)
					{
						data.physics_bone_map[target_bone] = 2;
					}
					else
					{
						data.physics_bone_map[target_bone] = 1;
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
				if (target_bone < 0) // Avoid dangerous map[-1] access, which auto-creates a buggy {-1, 0} mapping.
				{
					continue;
				}
				const int parent_bone = data.parent_index_map[target_bone];
				if (parent_bone < 0)
				{
					continue;
				}
				if (data.physics_bone_map.find(target_bone) != data.physics_bone_map.end() &&
					bone_to_rigid_map.find(parent_bone) != bone_to_rigid_map.end())
				{
					parent_physics_bone_list.push_back(parent_bone);
				}
			}
			for (int parent_bone : parent_physics_bone_list)
			{
				const pmx::PmxRigidBody& parent_rigid = data.pmx->rigid_bodies[bone_to_rigid_map[parent_bone]];
				if (parent_rigid.physics_calc_type == 0)
				{
					data.physics_bone_map[parent_bone] = 0;
				}
			}
		}
		// morph (face)
		const int morph_count = static_cast<int>(data.pmx->morphs.size());
		for (int i = 0; i < morph_count; ++i)
		{
			const pmx::PmxMorph& morph = data.pmx->morphs[i];
			std::string morph_name;
			oguna::EncodingConverter::Utf16ToCp932(morph.morph_name.c_str(), static_cast<int>(morph.morph_name.length()), &morph_name);
			data.morph_name_map[i] = morph_name;
		}
	}
}

static Imath::Matrix44<double> to_imath_matrix(const D3DMATRIX& mat)
{
	return Imath::Matrix44<double>(
		mat.m[0][0], mat.m[0][1], mat.m[0][2], mat.m[0][3],
		mat.m[1][0], mat.m[1][1], mat.m[1][2], mat.m[1][3],
		mat.m[2][0], mat.m[2][1], mat.m[2][2], mat.m[2][3],
		mat.m[3][0], mat.m[3][1], mat.m[3][2], mat.m[3][3]);
}

// Helper function: Calculate VMD bone frame based on bone index
static vmd::VmdBoneFrame calculate_bone_frame(
	int model_index,				// Model index (i)
	int bone_index,					// Bone index (k)
	int current_frame,				// Current frame number
	const FileDataForVMD& file_data // File data
)
{
	vmd::VmdBoneFrame bone_frame;
	bone_frame.frame = current_frame;
	bone_frame.name = ExpGetPmdBoneName(model_index, bone_index);

	// Get bone name for validation
	const char* bone_name = ExpGetPmdBoneName(model_index, bone_index);

	// Validate bone name mapping
	auto it = file_data.bone_name_map.find(bone_index);
	if (it == file_data.bone_name_map.end() || it->second != bone_name)
	{
		// If validation fails, return default bone_frame
		return bone_frame;
	}

	// Get initial position
	Imath::Vec3<float> initial_trans;
	if (file_data.pmd)
	{
		const pmd::PmdBone& bone = file_data.pmd->bones[bone_index];
		if (bone.bone_type == pmd::BoneType::Invisible)
		{
			return bone_frame; // Return default values
		}
		initial_trans.x = bone.bone_head_pos[0];
		initial_trans.y = bone.bone_head_pos[1];
		initial_trans.z = bone.bone_head_pos[2];
	}
	else if (file_data.pmx)
	{
		const pmx::PmxBone& bone = file_data.pmx->bones[bone_index];
		initial_trans.x = bone.position[0];
		initial_trans.y = bone.position[1];
		initial_trans.z = bone.position[2];
	}

	// Get transformation matrices
	Imath::Matrix44<double> world = to_imath_matrix(ExpGetPmdBoneWorldMat(model_index, bone_index));
	Imath::Matrix44<double> local = world;
	int parent_index = file_data.parent_index_map.count(bone_index) ? file_data.parent_index_map.at(bone_index) : -1;
	if (parent_index >= 0)
	{
		Imath::Matrix44<double> parent_world = to_imath_matrix(ExpGetPmdBoneWorldMat(model_index, parent_index));
		local = world * parent_world.inverse();
	}

	// Calculate VMD position
	// Step 1: Subtract own initial position
	bone_frame.position[0] = static_cast<float>(local[3][0]) - initial_trans.x;
	bone_frame.position[1] = static_cast<float>(local[3][1]) - initial_trans.y;
	bone_frame.position[2] = static_cast<float>(local[3][2]) - initial_trans.z;
	// Step 2: Add parent bone's initial position
	if (parent_index >= 0)
	{
		if (file_data.pmd && parent_index < static_cast<int>(file_data.pmd->bones.size()))
		{
			const pmd::PmdBone& parent_bone = file_data.pmd->bones[parent_index];
			bone_frame.position[0] += parent_bone.bone_head_pos[0];
			bone_frame.position[1] += parent_bone.bone_head_pos[1];
			bone_frame.position[2] += parent_bone.bone_head_pos[2];
		}
		else if (file_data.pmx && parent_index < static_cast<int>(file_data.pmx->bones.size()))
		{
			const pmx::PmxBone& parent_bone = file_data.pmx->bones[parent_index];
			bone_frame.position[0] += parent_bone.position[0];
			bone_frame.position[1] += parent_bone.position[1];
			bone_frame.position[2] += parent_bone.position[2];
		}
	}

	// Calculate rotation
	Imath::Matrix44<double> rotation_matrix = local;
	rotation_matrix[3][0] = rotation_matrix[3][1] = rotation_matrix[3][2] = 0.0;
	Imath::Quat<double> quat = Imath::extractQuat(rotation_matrix);

	// Normalize result
	quat.normalize();

	bone_frame.orientation[0] = static_cast<float>(quat.v.x);
	bone_frame.orientation[1] = static_cast<float>(quat.v.y);
	bone_frame.orientation[2] = static_cast<float>(quat.v.z);
	bone_frame.orientation[3] = static_cast<float>(quat.r);

	return bone_frame;
}

// Helper function: Calculate VMD face frame based on morph index
static vmd::VmdFaceFrame calculate_face_frame(
	int model_index,				// Model index (i)
	int morph_index,				// Morph index (m)
	int current_frame,				// Current frame number
	const FileDataForVMD& file_data // File data
)
{
	vmd::VmdFaceFrame face_frame;
	face_frame.frame = static_cast<uint32_t>(current_frame);

	// Get morph name
	const char* morph_name = ExpGetPmdMorphName(model_index, morph_index);
	face_frame.face_name = morph_name;

	// Validate morph name mapping
	auto it = file_data.morph_name_map.find(morph_index);
	if (it == file_data.morph_name_map.end() || it->second != morph_name)
	{
		// If validation fails, return default face_frame with 0 weight
		face_frame.weight = 0.0f;
		return face_frame;
	}

	// Get morph value from MMD
	face_frame.weight = ExpGetPmdMorphValue(model_index, morph_index);

	return face_frame;
}

static bool execute_vmd_export(const int currentframe)
{
	BridgeParameter::mutable_instance().is_exporting_with_mesh = false;

	VMDArchive& archive = VMDArchive::instance();

	const BridgeParameter& parameter = BridgeParameter::instance();
	const int pmd_num = ExpGetPmdNum();

	if (currentframe == parameter.start_frame)
	{
		for (int i = 0; i < pmd_num; ++i)
		{
			FileDataForVMD& file_data = archive.data_list.at(i);
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
		}
	}

	for (int i = 0; i < pmd_num; ++i)
	{
		FileDataForVMD& file_data = archive.data_list.at(i);
		const int bone_num = ExpGetPmdBoneNum(i);
		for (int k = 0; k < bone_num; ++k)
		{
			const char* bone_name = ExpGetPmdBoneName(i, k);

			if (!archive.has_bone_name_error && (bone_name == nullptr || strlen(bone_name) == 0))
			{
				archive.has_bone_name_error = true;
				ShowInvalidBoneNameError();
			}

			// Validate bone name mapping
			{
				auto it = file_data.bone_name_map.find(k);
				if (it == file_data.bone_name_map.end() || it->second != bone_name)
				{
					continue;
				}
			}

			// Export mode filtering
			const bool is_ik_effector_bone = file_data.ik_frame_bone_map.count(k) > 0;
			// const bool is_affected_by_ik = file_data.ik_bone_map.count(k) > 0;
			// const bool is_fuyo_effector_bone = file_data.fuyo_target_map.find(k) != file_data.fuyo_target_map.end();
			// const bool is_affected_by_fuyo = file_data.fuyo_bone_map.count(k) > 0;
			bool is_physics_bone = false;
			bool is_simulated_physics_bone = false;
			bool is_non_simulated_physics_bone = false;
			{
				auto it = file_data.physics_bone_map.find(k);
				is_physics_bone = (it != file_data.physics_bone_map.end());
				if (is_physics_bone)
				{
					if (it->second == 0) // ボーン追従
					{
						is_non_simulated_physics_bone = true;
					}
					else
					{
						is_simulated_physics_bone = true;
					}
				}
			}

			// Since IK is baked to FK, skip exporting IK bone motion keyframes
			if (is_ik_effector_bone)
			{
				if (!archive.export_ik_bone_animation)
				{
					continue;
				}
			}
			else // This is a FK bone
			{
				if (!is_simulated_physics_bone && archive.export_fk_bone_animation_mode == 0) // 0 = simulated physics bones only
				{
					continue;
				}
			}

			// Use helper function to calculate bone frame
			vmd::VmdBoneFrame bone_frame = calculate_bone_frame(i, k, currentframe, file_data);

			if (archive.export_fk_bone_animation_mode == 1) // 1 = All FK bones, keep 付与親 constraint (for MMD, MMD Tools)
			{
				// Remove grant parent influence if this is a grant child bone
				if (file_data.pmx && k < static_cast<int>(file_data.pmx->bones.size()))
				{
					const pmx::PmxBone& current_bone = file_data.pmx->bones[k];
					const uint16_t grant_flags = current_bone.bone_flag & 0x0300; // 0x0100 | 0x0200

					if (grant_flags && current_bone.grant_parent_index >= 0 &&
						current_bone.grant_parent_index < static_cast<int>(file_data.pmx->bones.size()))
					{
						// Calculate grant parent bone frame
						vmd::VmdBoneFrame grant_parent_frame = calculate_bone_frame(
							i, current_bone.grant_parent_index, currentframe, file_data);

						const float grant_weight = current_bone.grant_weight;

						// Remove position grant influence
						if (grant_flags & 0x0200) // Position grant
						{
							bone_frame.position[0] -= grant_parent_frame.position[0] * grant_weight;
							bone_frame.position[1] -= grant_parent_frame.position[1] * grant_weight;
							bone_frame.position[2] -= grant_parent_frame.position[2] * grant_weight;
						}

						// Remove rotation grant influence - using Imath::Quat
						if (grant_flags & 0x0100) // Rotation grant
						{
							// Create Imath quaternion objects
							Imath::Quatf current_quat(bone_frame.orientation[3],
													  bone_frame.orientation[0],
													  bone_frame.orientation[1],
													  bone_frame.orientation[2]);
							Imath::Quatf parent_quat(grant_parent_frame.orientation[3],
													 grant_parent_frame.orientation[0],
													 grant_parent_frame.orientation[1],
													 grant_parent_frame.orientation[2]);

							// Create identity quaternion for interpolation
							Imath::Quatf identity = Imath::Quatf::identity();

							// Use slerp to calculate scaled parent quaternion
							// scaled_parent_quat = slerp(identity, parent_quat, grant_weight)
							Imath::Quatf scaled_parent_quat = slerpShortestArc(identity, parent_quat, grant_weight);

							// Remove grant influence: current_pure = current * scaled_parent_quat^(-1)
							Imath::Quatf pure_quat = current_quat * scaled_parent_quat.inverse();

							// Normalize result
							pure_quat.normalize();

							// Convert back to VMD format (x, y, z, w)
							bone_frame.orientation[0] = pure_quat.v.x;
							bone_frame.orientation[1] = pure_quat.v.y;
							bone_frame.orientation[2] = pure_quat.v.z;
							bone_frame.orientation[3] = pure_quat.r;
						}
					}
				}
			}
			else // 2 = All FK bones, bake 付与親 constraint to FK
			{
				// Keep grant parent influence
				// The FK animation is already baked, do nothing
			}

			file_data.vmd->bone_frames.push_back(bone_frame);
		}

		// Handle IK frames for the first frame
		if (archive.add_turn_off_ik_keyframe && currentframe == parameter.start_frame)
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

		// morph (face)
		if (archive.export_morph_animation)
		{
			const int morph_num = ExpGetPmdMorphNum(i);
			for (int m = 0; m < morph_num; ++m)
			{
				const char* morph_name = ExpGetPmdMorphName(i, m);

				// Validate morph name mapping
				auto it = file_data.morph_name_map.find(m);
				if (it == file_data.morph_name_map.end() || it->second != morph_name)
				{
					continue;
				}

				// Use helper function to calculate face frame
				vmd::VmdFaceFrame face_frame = calculate_face_frame(i, m, currentframe, file_data);
				file_data.vmd->face_frames.push_back(face_frame);
			}
		}
	}

	return true;
}

// ---------------------------------------------------------------------------
PYBIND11_MODULE(mmdbridge_vmd, m)
{
	m.doc() = "MMD Bridge VMD export module";
	m.def("start_vmd_export", start_vmd_export,
		  py::arg("export_fk_bone_animation_mode"),
		  py::arg("export_ik_bone_animation"),
		  py::arg("add_turn_off_ik_keyframe"),
		  py::arg("export_morph_animation"));
	m.def("end_vmd_export", end_vmd_export);
	m.def("execute_vmd_export", execute_vmd_export);
}

#endif // WITH_VMD

// ---------------------------------------------------------------------------
// clang-format off
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
	void InitVMD() {}
	void DisposeVMD() {}
#endif // WITH_VMD
// clang-format on
