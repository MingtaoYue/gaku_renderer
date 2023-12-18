# ビルド
```
git clone https://github.com/MingtaoYue/gaku_renderer.git
cd gaku_renderer
mkdir build
cd build
cmake ..
cmake --build . -j
./gakurenderer ../obj/diablo3_pose/diablo3_pose.obj
```
# 説明
ラスタライズ手法を用いて実装したレンダリングソフトウェアです。
- 3Dモデルの情報を読み込む (Wavefront .objファイル)
- 設定したカメラと画面パラメータに基づいて、モデルのワールド座標を順次にモデル、カメラ、投影、ビューポート変換を行い、対応する画面座標を獲得
- Zバッファで深度情報を保存



