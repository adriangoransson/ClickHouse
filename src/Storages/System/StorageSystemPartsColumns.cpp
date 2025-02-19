#include "StorageSystemPartsColumns.h"

#include <Common/escapeForFileName.h>
#include <Columns/ColumnString.h>
#include <DataTypes/DataTypeString.h>
#include <DataTypes/DataTypesNumber.h>
#include <DataTypes/DataTypeDateTime.h>
#include <DataTypes/DataTypeDate.h>
#include <DataTypes/DataTypeArray.h>
#include <DataTypes/DataTypeNested.h>
#include <DataTypes/DataTypeNullable.h>
#include <DataTypes/NestedUtils.h>
#include <DataTypes/DataTypeUUID.h>
#include <Storages/VirtualColumnUtils.h>
#include <Databases/IDatabase.h>
#include <Parsers/queryToString.h>

namespace DB
{


StorageSystemPartsColumns::StorageSystemPartsColumns(const StorageID & table_id_)
    : StorageSystemPartsBase(table_id_,
    ColumnsDescription{
        {"partition",                                  std::make_shared<DataTypeString>()},
        {"name",                                       std::make_shared<DataTypeString>()},
        {"uuid",                                       std::make_shared<DataTypeUUID>()},
        {"part_type",                                  std::make_shared<DataTypeString>()},
        {"active",                                     std::make_shared<DataTypeUInt8>()},
        {"marks",                                      std::make_shared<DataTypeUInt64>()},
        {"rows",                                       std::make_shared<DataTypeUInt64>()},
        {"bytes_on_disk",                              std::make_shared<DataTypeUInt64>()},
        {"data_compressed_bytes",                      std::make_shared<DataTypeUInt64>()},
        {"data_uncompressed_bytes",                    std::make_shared<DataTypeUInt64>()},
        {"marks_bytes",                                std::make_shared<DataTypeUInt64>()},
        {"modification_time",                          std::make_shared<DataTypeDateTime>()},
        {"remove_time",                                std::make_shared<DataTypeDateTime>()},
        {"refcount",                                   std::make_shared<DataTypeUInt32>()},
        {"min_date",                                   std::make_shared<DataTypeDate>()},
        {"max_date",                                   std::make_shared<DataTypeDate>()},
        {"min_time",                                   std::make_shared<DataTypeDateTime>()},
        {"max_time",                                   std::make_shared<DataTypeDateTime>()},
        {"partition_id",                               std::make_shared<DataTypeString>()},
        {"min_block_number",                           std::make_shared<DataTypeInt64>()},
        {"max_block_number",                           std::make_shared<DataTypeInt64>()},
        {"level",                                      std::make_shared<DataTypeUInt32>()},
        {"data_version",                               std::make_shared<DataTypeUInt64>()},
        {"primary_key_bytes_in_memory",                std::make_shared<DataTypeUInt64>()},
        {"primary_key_bytes_in_memory_allocated",      std::make_shared<DataTypeUInt64>()},

        {"database",                                   std::make_shared<DataTypeString>()},
        {"table",                                      std::make_shared<DataTypeString>()},
        {"engine",                                     std::make_shared<DataTypeString>()},
        {"disk_name",                                  std::make_shared<DataTypeString>()},
        {"path",                                       std::make_shared<DataTypeString>()},

        {"column",                                     std::make_shared<DataTypeString>()},
        {"type",                                       std::make_shared<DataTypeString>()},
        {"column_position",                            std::make_shared<DataTypeUInt64>()},
        {"default_kind",                               std::make_shared<DataTypeString>()},
        {"default_expression",                         std::make_shared<DataTypeString>()},
        {"column_bytes_on_disk",                       std::make_shared<DataTypeUInt64>()},
        {"column_data_compressed_bytes",               std::make_shared<DataTypeUInt64>()},
        {"column_data_uncompressed_bytes",             std::make_shared<DataTypeUInt64>()},
        {"column_marks_bytes",                         std::make_shared<DataTypeUInt64>()},
        {"column_modification_time",                   std::make_shared<DataTypeNullable>(std::make_shared<DataTypeDateTime>())},

        {"serialization_kind",                         std::make_shared<DataTypeString>()},
        {"substreams",                                 std::make_shared<DataTypeArray>(std::make_shared<DataTypeString>())},
        {"filenames",                                  std::make_shared<DataTypeArray>(std::make_shared<DataTypeString>())},
        {"subcolumns.names",                           std::make_shared<DataTypeArray>(std::make_shared<DataTypeString>())},
        {"subcolumns.types",                           std::make_shared<DataTypeArray>(std::make_shared<DataTypeString>())},
        {"subcolumns.serializations",                  std::make_shared<DataTypeArray>(std::make_shared<DataTypeString>())},
        {"subcolumns.bytes_on_disk",                   std::make_shared<DataTypeArray>(std::make_shared<DataTypeUInt64>())},
        {"subcolumns.data_compressed_bytes",           std::make_shared<DataTypeArray>(std::make_shared<DataTypeUInt64>())},
        {"subcolumns.data_uncompressed_bytes",         std::make_shared<DataTypeArray>(std::make_shared<DataTypeUInt64>())},
        {"subcolumns.marks_bytes",                     std::make_shared<DataTypeArray>(std::make_shared<DataTypeUInt64>())},
    }
    )
{
}

void StorageSystemPartsColumns::processNextStorage(
    ContextPtr, MutableColumns & columns, std::vector<UInt8> & columns_mask, const StoragesInfo & info, bool has_state_column)
{
    /// Prepare information about columns in storage.
    struct ColumnInfo
    {
        String default_kind;
        String default_expression;
    };

    std::unordered_map<String, ColumnInfo> columns_info;
    for (const auto & column : info.storage->getInMemoryMetadataPtr()->getColumns())
    {
        ColumnInfo column_info;
        if (column.default_desc.expression)
        {
            column_info.default_kind = toString(column.default_desc.kind);
            column_info.default_expression = queryToString(column.default_desc.expression);
        }

        columns_info[column.name] = column_info;
    }

    /// Go through the list of parts.
    MergeTreeData::DataPartStateVector all_parts_state;
    MergeTreeData::DataPartsVector all_parts;
    all_parts = info.getParts(all_parts_state, has_state_column);
    for (size_t part_number = 0; part_number < all_parts.size(); ++part_number)
    {
        const auto & part = all_parts[part_number];
        auto part_state = all_parts_state[part_number];
        auto columns_size = part->getTotalColumnsSize();

        /// For convenience, in returned refcount, don't add references that was due to local variables in this method: all_parts, active_parts.
        auto use_count = part.use_count() - 1;

        auto min_max_date = part->getMinMaxDate();
        auto min_max_time = part->getMinMaxTime();

        auto index_size_in_bytes = part->getIndexSizeInBytes();
        auto index_size_in_allocated_bytes = part->getIndexSizeInAllocatedBytes();

        using State = MergeTreeDataPartState;

        size_t column_position = 0;
        for (const auto & column : part->getColumns())
        {
            ++column_position;
            size_t src_index = 0, res_index = 0;

            if (columns_mask[src_index++])
            {
                WriteBufferFromOwnString out;
                part->partition.serializeText(*info.data, out, format_settings);
                columns[res_index++]->insert(out.str());
            }

            if (columns_mask[src_index++])
                columns[res_index++]->insert(part->name);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(part->uuid);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(part->getTypeName());
            if (columns_mask[src_index++])
                columns[res_index++]->insert(part_state == State::Active);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(part->getMarksCount());

            if (columns_mask[src_index++])
                columns[res_index++]->insert(part->rows_count);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(part->getBytesOnDisk());
            if (columns_mask[src_index++])
                columns[res_index++]->insert(columns_size.data_compressed);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(columns_size.data_uncompressed);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(columns_size.marks);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(UInt64(part->modification_time));
            if (columns_mask[src_index++])
                columns[res_index++]->insert(UInt64(part->remove_time.load(std::memory_order_relaxed)));

            if (columns_mask[src_index++])
                columns[res_index++]->insert(UInt64(use_count));

            if (columns_mask[src_index++])
                columns[res_index++]->insert(min_max_date.first);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(min_max_date.second);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(static_cast<UInt32>(min_max_time.first));
            if (columns_mask[src_index++])
                columns[res_index++]->insert(static_cast<UInt32>(min_max_time.second));

            if (columns_mask[src_index++])
                columns[res_index++]->insert(part->info.partition_id);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(part->info.min_block);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(part->info.max_block);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(part->info.level);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(UInt64(part->info.getDataVersion()));
            if (columns_mask[src_index++])
                columns[res_index++]->insert(index_size_in_bytes);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(index_size_in_allocated_bytes);

            if (columns_mask[src_index++])
                columns[res_index++]->insert(info.database);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(info.table);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(info.engine);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(part->getDataPartStorage().getDiskName());
            if (columns_mask[src_index++])
            {
                /// The full path changes at clean up thread, so do not read it if parts can be deleted, avoid the race.
                if (part->isStoredOnDisk()
                    && part_state != State::Deleting && part_state != State::DeleteOnDestroy && part_state != State::Temporary)
                {
                    columns[res_index++]->insert(part->getDataPartStorage().getFullPath());
                }
                else
                    columns[res_index++]->insertDefault();
            }

            if (columns_mask[src_index++])
                columns[res_index++]->insert(column.name);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(column.type->getName());
            if (columns_mask[src_index++])
                columns[res_index++]->insert(column_position);

            auto column_info_it = columns_info.find(column.name);
            if (column_info_it != columns_info.end())
            {
                if (columns_mask[src_index++])
                    columns[res_index++]->insert(column_info_it->second.default_kind);
                if (columns_mask[src_index++])
                    columns[res_index++]->insert(column_info_it->second.default_expression);
            }
            else
            {
                if (columns_mask[src_index++])
                    columns[res_index++]->insertDefault();
                if (columns_mask[src_index++])
                    columns[res_index++]->insertDefault();
            }

            ColumnSize column_size = part->getColumnSize(column.name);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(column_size.data_compressed + column_size.marks);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(column_size.data_compressed);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(column_size.data_uncompressed);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(column_size.marks);
            if (columns_mask[src_index++])
            {
                if (auto column_modification_time = part->getColumnModificationTime(column.name))
                    columns[res_index++]->insert(UInt64(column_modification_time.value()));
                else
                    columns[res_index++]->insertDefault();
            }

            auto serialization = part->getSerialization(column.name);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(ISerialization::kindToString(serialization->getKind()));

            Array substreams;
            Array filenames;

            serialization->enumerateStreams([&](const auto & subpath)
            {
                auto substream = ISerialization::getFileNameForStream(column.name, subpath);
                auto filename = IMergeTreeDataPart::getStreamNameForColumn(column.name, subpath, part->checksums);

                substreams.push_back(std::move(substream));
                filenames.push_back(filename.value_or(""));
            });

            if (columns_mask[src_index++])
                columns[res_index++]->insert(substreams);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(filenames);

            Array subcolumn_names;
            Array subcolumn_types;
            Array subcolumn_serializations;
            Array subcolumn_bytes_on_disk;
            Array subcolumn_data_compressed_bytes;
            Array subcolumn_data_uncompressed_bytes;
            Array subcolumn_marks_bytes;

            IDataType::forEachSubcolumn([&](const auto & subpath, const auto & name, const auto & data)
            {
                /// We count only final subcolumns, which are represented by files on disk
                /// and skip intermediate subcolumns of types Tuple and Nested.
                if (isTuple(data.type) || isNested(data.type))
                    return;

                subcolumn_names.push_back(name);
                subcolumn_types.push_back(data.type->getName());
                subcolumn_serializations.push_back(ISerialization::kindToString(data.serialization->getKind()));

                ColumnSize size;
                NameAndTypePair subcolumn(column.name, name, column.type, data.type);

                auto stream_name = IMergeTreeDataPart::getStreamNameForColumn(subcolumn, subpath, part->checksums);
                if (stream_name)
                {
                    auto bin_checksum = part->checksums.files.find(*stream_name + ".bin");
                    if (bin_checksum != part->checksums.files.end())
                    {
                        size.data_compressed += bin_checksum->second.file_size;
                        size.data_uncompressed += bin_checksum->second.uncompressed_size;
                    }

                    auto mrk_checksum = part->checksums.files.find(*stream_name + part->index_granularity_info.mark_type.getFileExtension());
                    if (mrk_checksum != part->checksums.files.end())
                        size.marks += mrk_checksum->second.file_size;
                }

                subcolumn_bytes_on_disk.push_back(size.data_compressed + size.marks);
                subcolumn_data_compressed_bytes.push_back(size.data_compressed);
                subcolumn_data_uncompressed_bytes.push_back(size.data_uncompressed);
                subcolumn_marks_bytes.push_back(size.marks);

            }, ISerialization::SubstreamData(serialization).withType(column.type));

            if (columns_mask[src_index++])
                columns[res_index++]->insert(subcolumn_names);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(subcolumn_types);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(subcolumn_serializations);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(subcolumn_bytes_on_disk);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(subcolumn_data_compressed_bytes);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(subcolumn_data_uncompressed_bytes);
            if (columns_mask[src_index++])
                columns[res_index++]->insert(subcolumn_marks_bytes);

            if (has_state_column)
                columns[res_index++]->insert(part->stateString());
        }
    }
}

}
