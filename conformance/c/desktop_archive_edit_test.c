#define WinMain mosaic_desktop_test_WinMain
#include "../../native/src/mosaic_desktop.c"

static void free_test_entries(MosaicEntry *entries, size_t count) {
    if (!entries) return;
    for (size_t i = 0; i < count; ++i) free(entries[i].bytes);
    free(entries);
}

static int fail(const char *message) {
    fprintf(stderr, "desktop archive edit test failed: %s\n", message);
    return 1;
}

int main(int argc, char **argv) {
    MosaicEntry source[3];
    MosaicEntry unsafe_entry;
    MosaicEntry *parsed = NULL;
    size_t parsed_count = 0;
    int selected[] = {0};
    uint8_t *serialized = NULL;
    size_t serialized_len = 0;
    uint8_t *unsafe_serialized = NULL;
    size_t unsafe_len = 0;
    uint8_t *compressed = NULL;
    size_t compressed_len = 0;
    uint8_t *decompressed = NULL;
    char root_name[MAX_PATH];
    char error[160];
    char first_edited[MAX_PATH * 2] = {0};
    char second_edited[MAX_PATH * 2] = {0};
    MosaicArchiveHeader header;
    static const uint8_t child_data[] = {1, 2, 3};
    static const uint8_t keep_data[] = {4, 5};

    if (argc != 2) return fail("expected a repository-local archive path");
    ZeroMemory(source, sizeof(source));
    source[0].is_dir = 1;
    strcpy(source[0].path, "folder");
    strcpy(source[1].path, "folder\\child.txt");
    source[1].bytes = (uint8_t *)child_data;
    source[1].size = sizeof(child_data);
    strcpy(source[2].path, "keep.txt");
    source[2].bytes = (uint8_t *)keep_data;
    source[2].size = sizeof(keep_data);

    if (!archive_entry_is_selected(source, 3, 0, selected, 1) ||
        !archive_entry_is_selected(source, 3, 1, selected, 1) ||
        archive_entry_is_selected(source, 3, 2, selected, 1)) {
        return fail("folder selection did not include only its descendants");
    }

    DeleteFileA(argv[1]);
    if (!write_file(argv[1], keep_data, sizeof(keep_data)) ||
        !make_edited_archive_path(argv[1], first_edited, sizeof(first_edited)) ||
        !write_file(first_edited, keep_data, sizeof(keep_data)) ||
        !make_edited_archive_path(argv[1], second_edited, sizeof(second_edited)) ||
        strcmp(first_edited, second_edited) == 0) {
        DeleteFileA(argv[1]);
        DeleteFileA(first_edited);
        return fail("edited archive names were not collision-safe");
    }

    if (!serialize_entries(source, 3, "root", &serialized, &serialized_len) ||
        !parse_archive_v2(serialized, serialized_len, root_name, sizeof(root_name), &parsed, &parsed_count, error, sizeof(error)) ||
        parsed_count != 3 || strcmp(root_name, "root") != 0) {
        free(serialized);
        free_test_entries(parsed, parsed_count);
        return fail("valid archive payload did not round-trip");
    }
    free_test_entries(parsed, parsed_count);
    parsed = NULL;
    parsed_count = 0;
    {
        uint8_t *grown = (uint8_t *)realloc(serialized, serialized_len + 1);
        if (!grown) {
            free(serialized);
            return fail("out of memory");
        }
        serialized = grown;
    }
    serialized[serialized_len] = 0;
    if (parse_archive_v2(serialized, serialized_len + 1, root_name, sizeof(root_name), &parsed, &parsed_count, error, sizeof(error))) {
        free_test_entries(parsed, parsed_count);
        free(serialized);
        return fail("trailing archive payload was accepted");
    }

    ZeroMemory(&unsafe_entry, sizeof(unsafe_entry));
    strcpy(unsafe_entry.path, "..\\escape.txt");
    unsafe_entry.bytes = (uint8_t *)keep_data;
    unsafe_entry.size = sizeof(keep_data);
    if (!serialize_entries(&unsafe_entry, 1, "root", &unsafe_serialized, &unsafe_len)) {
        free(serialized);
        return fail("could not build unsafe-path fixture");
    }
    parsed = NULL;
    parsed_count = 0;
    if (parse_archive_v2(unsafe_serialized, unsafe_len, root_name, sizeof(root_name), &parsed, &parsed_count, error, sizeof(error))) {
        free_test_entries(parsed, parsed_count);
        free(serialized);
        free(unsafe_serialized);
        return fail("unsafe archive path was accepted");
    }

    if (compress_buffer(COMPRESS_ALGORITHM_XPRESS_HUFF, keep_data, sizeof(keep_data), &compressed, &compressed_len) != ERROR_SUCCESS ||
        decompress_blob_exact(COMPRESS_ALGORITHM_XPRESS_HUFF, compressed, compressed_len, sizeof(keep_data) + 1, &decompressed, NULL)) {
        free(decompressed);
        free(compressed);
        free(serialized);
        free(unsafe_serialized);
        return fail("decompressed-length mismatch was accepted");
    }
    ZeroMemory(&header, sizeof(header));
    header.magic = MOSAIC_ARCHIVE_MAGIC;
    header.version = MOSAIC_ARCHIVE_HEADER_VERSION;
    header.original_size = sizeof(keep_data);
    header.compressed_size = compressed_len;
    header.checksum32 = crc32_bytes(compressed, compressed_len);
    {
        uint8_t *archive = (uint8_t *)malloc(sizeof(header) + compressed_len);
        if (!archive) return fail("out of memory");
        memcpy(archive, &header, sizeof(header));
        memcpy(archive + sizeof(header), compressed, compressed_len);
        if (!archive_header_is_valid(&header, archive, sizeof(header) + compressed_len)) {
            free(archive);
            return fail("valid outer archive header was rejected");
        }
        header.version = MOSAIC_ARCHIVE_PAYLOAD_VERSION;
        memcpy(archive, &header, sizeof(header));
        if (archive_header_is_valid(&header, archive, sizeof(header) + compressed_len)) {
            free(archive);
            return fail("payload version was accepted as an outer archive version");
        }
        free(archive);
    }

    DeleteFileA(argv[1]);
    DeleteFileA(first_edited);
    free(compressed);
    free(serialized);
    free(unsafe_serialized);
    fprintf(stdout, "OK desktop archive edit test\n");
    return 0;
}
