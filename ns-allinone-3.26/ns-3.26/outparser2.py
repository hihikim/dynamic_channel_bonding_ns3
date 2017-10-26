import xlsxwriter

ap_dir_path = './output/ap/'
sta_dir_path = './output/sta/'

algorithms = ('opt','proposed','waterfall')

test_name = ''

class Parser:
	def __init__(self):
		self.workbook = xlsxwriter.Workbook("test_" + test_name + ".xlsx")

	def Parsing(self):
		for algo in algorithms :
			ap_path = ap_dir_path + algo + '/'
			sta_path = sta_dir_path + algo + '/'
			self.worksheet = self.workbook.add_worksheet(algo)
			self.ap_file = open(ap_path + test_name,'r')
			self.sta_file = open(sta_path + test_name,'r')
			self.PrintToSheet()
			self.ap_file.close()
			self.sta_file.close()

		self.workbook.close()


	def PrintToSheet(self):
		worksheet = self.worksheet
		row = 1
		col = 2
		is_first = False
		first_time = True
		ap_row = 0

		ap_index = []

		for line in self.ap_file.readlines():
			if not line.find('---') > -1:
				val_index = line.find(' :')

				if val_index == -1 :
					continue

				token = line[0:val_index]
				value = line[val_index+3:]

				value = value.replace('\n','')


				if token == 'Time' :
					if first_time :
						is_first = True
						first_time = False

					worksheet.write_string(1, col, value)

				elif token == 'index' :
					row += 1
					if  is_first :
						worksheet.write_string(row, 0,"AP "+ value)
						worksheet.write(row, 1, 'throughput')
						worksheet.write(row+1, 1, 'average throughput')
						worksheet.write(row+2, 1, 'minimum throughput')
						worksheet.write(row+3, 1, 'maximum throughput')
						worksheet.write(row+4, 1, 'total throughput/demand(%)')
						ap_index.append(row+4)


				elif token in ('throughput', 'average throughput', 'minimum throughput', 'maximum throughput', 'total throughput / demand(%)') :
					value = float(value)
					worksheet.write(row, col, value)
					row += 1


				elif token.find('Channel') > -1:
					if is_first :
						worksheet.write(row, 1, token)

					value = float(value)
					worksheet.write(row, col, value)
					row += 1


			else :
				col += 1
				is_first = False
				if ap_row < row :
					ap_row = row
				row = 1

		query = "= AVERAGE("

		is_first = True

		for row_val in ap_index:
			if not is_first:
				query += ', '
			else :
				is_first = False

			query += self.get_cell_addr(row_val,col-1)

		query += ')'

		worksheet.write(2, col +3, query)



		row = ap_row +2
		col = 2
		is_first = False
		first_time = True

		sta_index = []

		for line in self.sta_file.readlines():
			if not line.find('---') > -1:
				val_index = line.find(' :')

				if val_index == -1 :
					continue

				token = line[0:val_index]
				value = line[val_index+3:]

				value = value.replace('\n','')

				if token == 'Time' :
					if first_time :
						is_first = True
						first_time = False

					worksheet.write(ap_row+2, col, value)

				elif token == 'index' :
					row += 1
					if  is_first :
						worksheet.write(row, 0, "STA "+ value )
						worksheet.write(row, 1, 'throughput')
						worksheet.write(row+1, 1, 'average throughput')
						worksheet.write(row+2, 1, 'minimum throughput')
						worksheet.write(row+3, 1, 'maximum throughput')
						worksheet.write(row+4, 1, 'throughput/demand(%)')
						sta_index.append(row+4)


				elif token in ('throughput', 'average throughput', 'minimum throughput', 'maximum throughput', 'throughput/demand(%)') :
					value = float(value)
					worksheet.write(row, col, value)
					row += 1


			else :
				col += 1
				is_first = False
				row = ap_row + 2



		query = "= AVERAGE("

		is_first = True

		for row_val in sta_index:
			if not is_first:
				query += ', '
			else :
				is_first = False

			query += self.get_cell_addr(row_val,col-1)

		query += ')'


		worksheet.write(3, col +3, query)

	def get_cell_addr(self,row,col):
		row += 1
		result = "";

		while True :
			remain = col % 26
			col /=  26
			remain += ord("A")

			result = str(chr(remain)) + result
			if col is 0 :
				break;

		result += str(row)

		return result


test_name = input('input the test number : ')

parser = Parser()
parser.Parsing()
