#include "exprtk.hpp"
#include "algorithm.h"

#include <regex>
#include <map>
#include <set>

Coordinate::Coordinate(Function& function, std::map<std::string, double> const& initializedSymbols)
    :symbols(initializedSymbols),
     function(function)
{
    for(auto const& symbol : initializedSymbols)
    {
        function.setValue(symbol.first, symbol.second);
    }

    value = function.calculateExpression();
}

Function& Coordinate::getFunction() const
{
    return function;
}

double Coordinate::getValue() const
{
    return value;
}

Function::Function(std::string const& expression)
{
    std::regex symbols("x[0-9]+");

    auto begin = std::sregex_iterator(expression.begin(), expression.end(), symbols);
    auto end = std::sregex_iterator();

    for (std::sregex_iterator i = begin; i != end; ++i)
    {
        uniqueSymbols_[(*i).str()] = 0.0;
    }

    for(auto & symbolPair : uniqueSymbols_)
    {
        symbol_table_.add_variable(symbolPair.first, uniqueSymbols_[symbolPair.first]);
    }

    expression_.register_symbol_table(symbol_table_);
    parser_.compile(expression, expression_);
}

bool Function::setValue(std::string const& symbol, double const& newValue)
{
    auto it = uniqueSymbols_.find(symbol);
    if(it == uniqueSymbols_.end())
        return false;
    it->second = newValue;
    return true;
}

double Function::calculateExpression()
{
    return expression_.value();
}

void Algorithm::putSymbolsToTable()
{
    auto extractSymbolsIntoSet = +[](std::string const& expression) -> std::set<std::string>
    {
        std::regex regex("x[0-9]+");

        auto begin = std::sregex_iterator(expression.begin(), expression.end(), regex);
        auto end = std::sregex_iterator();

        std::set<std::string> symbols;

        for(std::sregex_iterator i = begin; i != end; ++i)
        {
            symbols.insert((*i).str());
        }
        return symbols;
    };

    auto firstFunctionSymbols = extractSymbolsIntoSet(std::string(ui->function1->text().toUtf8().constData()));
    auto secondFunctionSymbols = extractSymbolsIntoSet(std::string(ui->function2->text().toUtf8().constData()));

    firstFunctionSymbols.insert(secondFunctionSymbols.begin(), secondFunctionSymbols.end());

    ui->tableWidget->setRowCount(firstFunctionSymbols.size());
    ui->tableWidget->setColumnCount(4);

    ui->tableWidget->setHorizontalHeaderLabels(QStringList{"Symbol", "Min value", "Max value", "Result"});
    // ui->tableWidget->setVerticalHeaderLabels(QStringList{"x1", "x2", "x3", "x4"});

    auto i = 0u;
    for(auto const& symbol : firstFunctionSymbols)
    {
        auto *item = new QTableWidgetItem(QString::fromStdString(symbol));
        ui->tableWidget->setItem(i, 0, item);
        item->setFlags(item->flags() ^ Qt::ItemIsEditable);
        i++;
    }
}

bool isDominated(std::vector<Point> setOfPoints, Point comparePoint)
{
    return std::find_if(setOfPoints.begin(), setOfPoints.end(), [&](Point const& pointInCollection)
    {
        return comparePoint.x.getValue() > pointInCollection.x.getValue() and comparePoint.y.getValue() > pointInCollection.y.getValue();

    }) != setOfPoints.end();
}

double norm(Point firstPoint, Point secondPoint)
{
    auto xLength = firstPoint.x.getValue() - secondPoint.x.getValue();
    auto yLength = firstPoint.y.getValue() - secondPoint.y.getValue();

    return std::sqrt(std::pow(xLength, 2) + std::pow(yLength, 2));
}

std::size_t numberOfPointsInCircleWithNicheRadius(std::vector<Point> compareSet, Point point, double nicheRadius)
{
    std::size_t numberOfPoints = 0;
    for(auto const& comparePoint : compareSet)
    {
        if (norm(comparePoint, point) < nicheRadius)
        {
            numberOfPoints++;
        }
    }
    return numberOfPoints;
}

std::set<std::string> Algorithm::extractSymbols(Function& function)
{
    std::set<std::string> symbols;
    static std::map<std::string, double> emptySymbols;
    Coordinate coordinate(function, emptySymbols);

    for(auto const& symbol : coordinate.symbols)
    {
        symbols.insert(symbol.first);
    }
    return symbols;
}

std::vector<Point> Algorithm::generatePoints(Function& firstFunction, Function& secondFunction, std::size_t numberOfPoints)
{
    std::vector<Point> points;
    for(std::size_t i = 0; i < numberOfPoints; ++i)
    {
        auto firstFunctionSymbols = extractSymbols(firstFunction);
        auto secondFunctionSymbols = extractSymbols(secondFunction);

        std::set<std::string> functionSymbols;
        for(auto const& symbol : firstFunctionSymbols)
        {
            functionSymbols.insert(symbol);
        }

        for(auto const& symbol : secondFunctionSymbols)
        {
            functionSymbols.insert(symbol);
        }

        std::map<std::string, double> symbolsForFirstFunction;
        std::map<std::string, double> symbolsForSecondFunction;

        for(auto const& symbol : functionSymbols)
        {
            auto min = constraints[symbol].min;
            auto max = constraints[symbol].max;

            auto generatedSymbolValue = generator.generateDouble(min, max);

            bool symbolRequiredByFirstFunction = firstFunctionSymbols.find(symbol) != firstFunctionSymbols.end();
            bool symbolRequiredBySecondFunction = secondFunctionSymbols.find(symbol) != secondFunctionSymbols.end();

            if(symbolRequiredByFirstFunction)
            {
                symbolsForFirstFunction[symbol] = generatedSymbolValue;
            }

            if(symbolRequiredBySecondFunction)
            {
                symbolsForSecondFunction[symbol] = generatedSymbolValue;
            }
        }
    }

    return points;
}

Point Algorithm::crossover(Point const& firstPoint, Point const& secondPoint)
{
    std::map<std::string, double> newXSymbols;
    for(auto const& symbol : firstPoint.x.symbols)
    {
        const auto symbolName = symbol.first;
        auto valueOfXInSecond = secondPoint.x.symbols.at(symbolName);

        auto newValue = (symbol.second + valueOfXInSecond)/2;

        newXSymbols[symbolName] = newValue;
    }

    std::map<std::string, double> newYSymbols;
    for(auto const& symbol : firstPoint.x.symbols)
    {
        auto& symbolName = symbol.first;
        auto valueOfYInSecond = secondPoint.x.symbols.at(symbolName);

        auto newValue = (symbol.second + valueOfYInSecond)/2;

        newYSymbols[symbolName] = newValue;
    }

    Coordinate coordinateX(firstPoint.x.getFunction(), newXSymbols);
    Coordinate coordinateY(firstPoint.y.getFunction(), newYSymbols);

    return Point(std::move(coordinateX), std::move(coordinateY));
}

Point Algorithm::mutate(Point const& point)
{
    auto xSymbolsOfBasedPoint = point.x.symbols;
    auto ySymbolsOfBasedPoint = point.y.symbols;

    auto mutateSymbol = [&](auto& setOfSymbols)
    {
        auto symbolIndex = generator.generateInt(0, setOfSymbols.size());

        auto symbolIt = setOfSymbols.begin();
        std::advance(symbolIt, symbolIndex);

        auto multiplicand = generator.generateDouble(-1, 1);
        (*symbolIt).second = (*symbolIt).second + (*symbolIt).second * multiplicand;
    };

    mutateSymbol(xSymbolsOfBasedPoint);
    mutateSymbol(ySymbolsOfBasedPoint);

    Coordinate coordinateX(point.x.getFunction(), xSymbolsOfBasedPoint);
    Coordinate coordinateY(point.y.getFunction(), ySymbolsOfBasedPoint);

    return Point(std::move(coordinateX), std::move(coordinateY));
}

void Algorithm::startCalculations()
{
    Function firstFunction(std::string(ui->function1->text().toUtf8().constData()));
    Function secondFunction(std::string(ui->function2->text().toUtf8().constData()));

    constraints["x1"] = Constraint(1.0, 3.0);
    constraints["x2"] = Constraint(-1.0, 7.0);

    // ui->function2->setText(QString::fromStdString(std::to_string(firstFunction.calculateExpression())) +
    //                       QString::fromStdString(std::to_string(secondFunction.calculateExpression())));

    auto pm = ui->mutationProbability->text().toDouble();
    auto pc = ui->crossingProbability->text().toDouble();
    auto T = ui->maxGenertation->text().toUInt();
    auto N = ui->populationSize->text().toUInt();
    auto sigma = ui->sigma->text().toDouble();

    auto tdom = 7u;

    std::vector<Point> p0 = generatePoints(firstFunction, secondFunction, N);

    for (unsigned t = 1; t < T; ++t)
    {
        std::vector<Point> temporarySet;

        for(std::size_t i = 0; i < N; i++)
        {
            auto firstPoint = p0[generator.generateInt(0, p0.size() - 1)];
            auto secondPoint = p0[generator.generateInt(0, p0.size() - 1)];

            std::vector<Point> compareSet = generatePoints(firstFunction, secondFunction, tdom);

            if(not isDominated(compareSet, firstPoint) and isDominated(compareSet, secondPoint))
            {
                temporarySet.push_back(firstPoint);
            }
            else if(isDominated(compareSet, firstPoint) and not isDominated(compareSet, secondPoint))
            {
                temporarySet.push_back(secondPoint);
            }
            else
            {
                auto nFirstPoint = numberOfPointsInCircleWithNicheRadius(compareSet, firstPoint, sigma);
                auto nSecondPoint = numberOfPointsInCircleWithNicheRadius(compareSet, secondPoint, sigma);

                if (nFirstPoint < nSecondPoint)
                {
                    temporarySet.push_back(firstPoint);
                }
                else
                {
                    temporarySet.push_back(secondPoint);
                }
            }
        }

        // TODO: VEGA 3, 4, 5
        std::vector<Point> crossoverSet;

        for(std::size_t i = 0; i < std::floor(N/2); i++)
        {
            auto firstPos = generator.generateInt(0, temporarySet.size() - 1);
            auto firstPoint = p0[firstPos];
            //   temporarySet.erase(temporarySet.begin() + firstPos);

            auto secondPos = generator.generateInt(0, temporarySet.size() - 1);
            auto secondPoint = p0[secondPos];
            //   temporarySet.erase(temporarySet.begin() + secondPos);

            auto decisionVariable = generator.generateDouble(0,100);
            bool shouldCross = decisionVariable < pc;

            if(shouldCross)
            {
                auto newPoint = crossover(firstPoint, secondPoint);
                crossoverSet.push_back(newPoint);
            }
        }

        std::vector<Point> mutationSet;

        for (auto const& crossoverPoint : crossoverSet)
        {
            auto decisionVariable = generator.generateDouble(0,100);
            bool shouldMutate = decisionVariable < pm;

            if(shouldMutate)
            {
                auto newPoint = mutate(crossoverPoint);
                mutationSet.push_back(newPoint);
            }
        }

        p0.clear();
        p0.swap(mutationSet);
    }
}