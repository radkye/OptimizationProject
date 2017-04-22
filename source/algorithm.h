#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <string>
#include "exprtk.hpp"
#include <QMainWindow>
#include <QObject>
#include <ui_mainwindow.h>
#include "random.hpp"

using symbol_table_t = exprtk::symbol_table<double>;
using expression_t   = exprtk::expression<double>;
using parser_t       = exprtk::parser<double>;

namespace Ui {
  class MainWindow;
}

class Function
{
    symbol_table_t symbol_table_;
    parser_t parser_;
    expression_t expression_;

    std::map<std::string, double> uniqueSymbols_;
public:
    Function(std::string const& expression);
    bool setValue(std::string const& symbol, double const& newValue);
    double calculateExpression();
};

struct Coordinate
{
    Coordinate(Function& function, std::map<std::string, double> const& initializedSymbols);

    Function& getFunction() const;
    double getValue() const;

    const std::map<std::string, double> symbols;
private:
    double value;
    Function& function;
};

struct Point
{
    Point(Coordinate const& x, Coordinate const& y) :x(x), y(y) {}
    const Coordinate x,y;
};

struct Constraint
{
    Constraint(double min = 0.0, double max = 0.0) :min(min), max(max) {}
    double min, max;
};

class Algorithm :public QObject
{
    Q_OBJECT

    std::map<std::string, Constraint> constraints;
    RandomGenerator generator;
    Ui::MainWindow* ui;
public:
    Algorithm(Ui::MainWindow* ui) :ui(ui) {}
    virtual ~Algorithm() {};

    std::set<std::string> extractSymbols(Function& function);
    std::vector<Point> generatePoints(Function& firstFunction, Function& secondFunction, std::size_t numberOfPoints);

    Point crossover(Point const& firstPoint, Point const& secondPoint);
    Point mutate(Point const& firstPoint);
public slots:
    void startCalculations();
    void putSymbolsToTable();
};

#endif // ALGORITHM_H