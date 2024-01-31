python convert_single.py /data/zhouliangy/marker-master/to_hkust/fin/100.pdf /data/zhouliangy/marker-master/debug_out/100_out.md --parallel_factor 2 --max_pages 10
python convert_single.py /data/zhouliangy/x/xiaoludechitang0000bunt.pdf /data/zhouliangy/marker-master/debug_out/xiaoludechitang.md --parallel_factor 1 --max_pages 40
python convert.py /data/zhouliangy/fd /data/zhouliangy/fd_marker --workers 1 --metadata_file /data/zhouliangy/marker-master/fd_meta.json # --max 10  --metadata_file /path/to/metadata.json --min_length 10000
MIN_LENGTH=10000 
METADATA_FILE=./meta.json NUM_DEVICES=8 NUM_WORKERS=1 bash chunk_convert.sh /data/zhouliangy/marker-master/to_hkust/all /data/zhouliangy/marker-master/debug_out/all_worker1
MIN_LENGTH=10000 

METADATA_FILE=../anna_language_predictions.json NUM_DEVICES=6 NUM_WORKERS=1 bash chunk_convert.sh /data/zhouliang/anna_error /data/zhouliangy/anna_error_default

# python convert.py /data/zhouliangy/marker-master/to_hkust/all /data/zhouliangy/demo_out --workers 3 --metadata_file /data/zhouliangy/marker-master/meta.json
python convert.py /data/zhouliangy/marker-master/pdf_csmath /data/zhouliangy/demo_out/ --worker 3 --metadata_file /data/zhouliangy/marker-master/pdf_csmath.json
python convert.py /data/zhouliangy/marker-master/to_hkust/all /data/zhouliangy/book_out/ --worker 3 --metadata_file /data/zhouliangy/marker-master/to_all.json

METADATA_FILE=./china_arxiv_errors.json NUM_DEVICES=8 NUM_WORKERS=1 bash chunk_convert.sh /data/zhouliangy/china_arxiv_errors /data/zhouliangy/china_arxiv_marker