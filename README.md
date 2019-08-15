# libc7 r1.0.0

C言語によるプログラミングの基本要素を集めたライブラリです。
ターゲットのC規格は C99 です。

## 変更履歴

### r1.0.0

#### c7coroutine

- ジェネレータ終了時のハンドリングを可能にした。

#### c7mlog

- c7status に保存したステータス情報をまとめて記録するAPIを追加。

#### c7status

- 単一のステータスコードを文字列に変換するAPIを追加。

#### c7thread

- ビットマスク型の同期機構を追加。

### r0.3.0

#### c7tpool

- タスク関数終了時のハンドリング機能を追加。
- タスク関数の大域脱出やタスク関数への引数取得関数を追加。

### r0.2.1

- c7prompt が git のブランチ名を取得できない場合にド終了値 1 で終了した問題を修正。

## 開発環境

おもに次の3つの環境で試しています。

OS|コンパイラ
:-|:-
CentOS7/x86_64 | gcc 4.8.5
CentOS6/x86_64 | gcc 4.4.7
Cygwin/x86_64 on Windows 8.1 | gcc 7.4.0

## ライブラリのドキュメント

[Doxygen](http://www.doxygen.jp/) で生成したドキュメントを github.io 上に準備しました。<BR>
こちらからどうぞ [:point_right: libc7](https://ccldaout.github.io/libc7/)

## ビルド

この README.md のあるディレクトリで make を実行します。
デフォルトでは ../build というディレクトリを作成し、そこを基点として bin や include, lib などのサブディレクトリが作成されます。
これを変更する手軽な方法は C7_OUT_ROOT 変数を定義して make を実行することです。
次は、../build の代わりに $HOME/opt を基点とする例です。

```sh
$ make C7_OUT_ROOT=$HOME/opt
```

## 生成されるものの概要

ディレクトリ|ファイル|説明
:-|:-|-
bin|c7dconf|dconf機能の設定確認・変更用のコマンド
bin|c7prompt|ディレクトリとgitのブランチ名をPS1への組み込み用に出力するコマンド
bin|c7mlog|mlog機能のログ確認コマンド
bin|c7npipe|標準入出→c7npipe-\[TCP\]-c7npipe→標準出力
bin|c7regrep|正規表現と置換パターンによる置換コマンド
lib|libc7.so|libc7ライブラリ。(Cygwinでは bin に libc7.dll)
lib/python2|stsgen.py|statusコードを文字列に変換するために必要な定義を生成する。
